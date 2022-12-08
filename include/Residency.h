// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    // Used to contain waits that must be satisfied before a pinned ManagedObject can be unpinned.
    class PinWaits
    {
    public:
        class PinWait
        {
        public:
            PinWait(ID3D12Fence* pFence, UINT64 value)
                : m_spFence(pFence), m_value(value)
            {
            }

            PinWait(const PinWait& pinWait) = default;
            ~PinWait() = default;

            CComPtr<ID3D12Fence> m_spFence;
            UINT64 m_value;
        };

        ~PinWaits() { ClearPinWaits(); }

        void ClearPinWaits()
        {
            m_pinWaits.clear();
        }

        void AddPinWaits(UINT NumSync, _In_reads_(NumSync) UINT64* pSignalValues, _In_reads_(NumSync) ID3D12Fence** ppFences)
        {
            m_pinWaits.reserve(m_pinWaits.size() + NumSync); // throw( bad_alloc);
            for (UINT i(0); i < NumSync; ++i)
            {
                m_pinWaits.emplace_back(ppFences[i], pSignalValues[i]);
            }
        }

        bool CheckPinWaits()
        {
            m_pinWaits.erase(std::remove_if(m_pinWaits.begin(), m_pinWaits.end(), [](PinWait& pinWait)
                {
                    return (pinWait.m_spFence->GetCompletedValue() >= pinWait.m_value);
                }), 
                m_pinWaits.end());

            return m_pinWaits.size() > 0;
        }

    private:
        std::vector<PinWait> m_pinWaits;
    };

    // Used to track meta data for each object the app potentially wants
    // to make resident or evict.
    class ManagedObject
    {
    public:
        enum class RESIDENCY_STATUS
        {
            RESIDENT,
            EVICTED
        };

        ManagedObject() = default;
        ~ManagedObject()
        {
#if TRANSLATION_LAYER_DBG
            for (bool val : CommandListsUsedOn)
            {
                assert(!val);
            }
#endif
        }

        void Initialize(ID3D12Pageable* pUnderlyingIn, UINT64 ObjectSize)
        {
            assert(pUnderlying == nullptr);
            pUnderlying = pUnderlyingIn;
            Size = ObjectSize;
        }

        inline bool IsInitialized() { return pUnderlying != nullptr; }

        bool IsPinned() { return PinCount > 0 || m_pinWaits.CheckPinWaits(); }
        void Pin() { ++PinCount; }
        void AddPinWaits(UINT NumSync, _In_reads_(NumSync) UINT64* pSignalValues, _In_reads_(NumSync) ID3D12Fence** ppFences) { return m_pinWaits.AddPinWaits(NumSync, pSignalValues, ppFences); }
        void UnPin() { assert(PinCount > 0);  --PinCount; }

        // Wether the object is resident or not
        RESIDENCY_STATUS ResidencyStatus = RESIDENCY_STATUS::RESIDENT;

        // The underlying D3D Object being tracked
        ID3D12Pageable* pUnderlying = nullptr;
        // The size of the D3D Object in bytes
        UINT64 Size = 0;

        UINT64 LastUsedFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
        UINT64 LastUsedTimestamp = 0;

        // This is used to track which open command lists this resource is currently used on.
        // + 1 for transient residency sets.
        bool CommandListsUsedOn[(UINT)COMMAND_LIST_TYPE::MAX_VALID + 1] = {};

        // Linked list entry
        LIST_ENTRY ListEntry;

        // Pinning an object prevents eviction.  Callers must seperately make resident as usual.
        UINT32 PinCount = 0;

        PinWaits m_pinWaits;
    };

    // This represents a set of objects which are referenced by a command list i.e. every time a resource
    // is bound for rendering, clearing, copy etc. the set must be updated to ensure the it is resident 
    // for execution.
    class ResidencySet
    {
        friend class ResidencyManager;
    public:

        static const UINT32 InvalidIndex = (UINT32)-1;

        ResidencySet() = default;
        ~ResidencySet() = default;

        // Returns true if the object was inserted, false otherwise
        inline bool Insert(ManagedObject* pObject)
        {
            assert(IsOpen);
            assert(CommandListIndex != InvalidIndex);

            // If we haven't seen this object on this command list mark it
            if (pObject->CommandListsUsedOn[CommandListIndex] == false)
            {
                pObject->CommandListsUsedOn[CommandListIndex] = true;
                Set.push_back(pObject);

                return true;
            }
            else
            {
                return false;
            }
        }

        HRESULT Open(UINT commandListType)
        {
            // It's invalid to open a set that is already open
            if (IsOpen)
            {
                return E_INVALIDARG;
            }

            assert(CommandListIndex == InvalidIndex);
            CommandListIndex = commandListType;

            Set.clear();

            IsOpen = true;
            return S_OK;
        }

        HRESULT Close()
        {
            if (IsOpen == false)
            {
                return E_INVALIDARG;
            }

            for (auto pObject : Set)
            {
                pObject->CommandListsUsedOn[CommandListIndex] = false;
            }

            CommandListIndex = InvalidIndex;
            IsOpen = false;

            return S_OK;
        }

    private:
        UINT32 CommandListIndex = InvalidIndex;
        std::vector<ManagedObject*> Set;
        bool IsOpen = false;
    };

    namespace Internal
    {
        struct Fence
        {
            Fence(UINT64 StartingValue = 0) : FenceValue(StartingValue)
            {
            }

            HRESULT Initialize(ID3D12Device* pDevice)
            {
                HRESULT hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
                assert(SUCCEEDED(hr));

                return hr;
            }

            void Destroy()
            {
                pFence.Release();
            }

            HRESULT GPUWait(ID3D12CommandQueue* pQueue)
            {
                HRESULT hr = pQueue->Wait(pFence, FenceValue);
                assert(SUCCEEDED(hr));
                return hr;
            }

            inline void Increment()
            {
                FenceValue++;
            }

            CComPtr<ID3D12Fence> pFence;
            UINT64 FenceValue;
        };

        // A Least Recently Used Cache. Tracks all of the objects requested by the app so that objects
        // that aren't used freqently can get evicted to help the app stay under buget.
        class LRUCache
        {
        public:
            LRUCache() :
                NumResidentObjects(0),
                NumEvictedObjects(0),
                ResidentSize(0)
            {
                InitializeListHead(&ResidentObjectListHead);
                InitializeListHead(&EvictedObjectListHead);
            };

            void Insert(ManagedObject* pObject)
            {
                if (pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT)
                {
                    InsertHeadList(&ResidentObjectListHead, &pObject->ListEntry);
                    NumResidentObjects++;
                    ResidentSize += pObject->Size;
                }
                else
                {
                    InsertHeadList(&EvictedObjectListHead, &pObject->ListEntry);
                    NumEvictedObjects++;
                }
            }

            void Remove(ManagedObject* pObject)
            {
                RemoveEntryList(&pObject->ListEntry);
                if (pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT)
                {
                    NumResidentObjects--;
                    ResidentSize -= pObject->Size;
                }
                else
                {
                    NumEvictedObjects--;
                }
            }

            // When an object is used by the GPU we move it to the end of the list.
            // This way things closer to the head of the list are the objects which
            // are stale and better candidates for eviction
            void ObjectReferenced(ManagedObject* pObject)
            {
                assert(pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT);

                RemoveEntryList(&pObject->ListEntry);
                InsertTailList(&ResidentObjectListHead, &pObject->ListEntry);
            }

            void MakeResident(ManagedObject* pObject)
            {
                assert(pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::EVICTED);

                pObject->ResidencyStatus = ManagedObject::RESIDENCY_STATUS::RESIDENT;
                RemoveEntryList(&pObject->ListEntry);
                InsertTailList(&ResidentObjectListHead, &pObject->ListEntry);

                NumEvictedObjects--;
                NumResidentObjects++;
                ResidentSize += pObject->Size;
            }

            void Evict(ManagedObject* pObject)
            {
                assert(pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT);
                assert(!pObject->IsPinned());

                pObject->ResidencyStatus = ManagedObject::RESIDENCY_STATUS::EVICTED;
                RemoveEntryList(&pObject->ListEntry);
                InsertTailList(&EvictedObjectListHead, &pObject->ListEntry);

                NumResidentObjects--;
                ResidentSize -= pObject->Size;
                NumEvictedObjects++;
            }

            // Evict all of the resident objects used in sync points up to the specficied one (inclusive)
            void TrimToSyncPointInclusive(INT64 CurrentUsage, INT64 CurrentBudget, ID3D12Pageable** EvictionList, UINT32& NumObjectsToEvict, UINT64 FenceValues[])
            {
                NumObjectsToEvict = 0;

                LIST_ENTRY* pResourceEntry = ResidentObjectListHead.Flink;
                while (pResourceEntry != &ResidentObjectListHead)
                {
                    ManagedObject* pObject = CONTAINING_RECORD(pResourceEntry, ManagedObject, ListEntry);

                    if (CurrentUsage < CurrentBudget)
                    {
                        return;
                    }
                    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
                    {
                        if (pObject->LastUsedFenceValues[i] > FenceValues[i])
                        {
                            return;
                        }
                    }

                    assert(pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT);

                    if (pObject->IsPinned())
                    {
                        pResourceEntry = pResourceEntry->Flink;
                    }
                    else
                    {
                        EvictionList[NumObjectsToEvict++] = pObject->pUnderlying;
                        Evict(pObject);

                        CurrentUsage -= pObject->Size;

                        pResourceEntry = ResidentObjectListHead.Flink;
                    }
                }
            }

            // Trim all objects which are older than the specified time
            void TrimAgedAllocations(UINT64 FenceValues[], ID3D12Pageable** EvictionList, UINT32& NumObjectsToEvict, UINT64 CurrentTimeStamp, UINT64 MinDelta)
            {
                LIST_ENTRY* pResourceEntry = ResidentObjectListHead.Flink;
                while (pResourceEntry != &ResidentObjectListHead)
                {
                    ManagedObject* pObject = CONTAINING_RECORD(pResourceEntry, ManagedObject, ListEntry);

                    if (CurrentTimeStamp - pObject->LastUsedTimestamp <= MinDelta) // Don't evict things which have been used recently
                    {
                        return;
                    }
                    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
                    {
                        if (pObject->LastUsedFenceValues[i] > FenceValues[i])
                        {
                            return;
                        }
                    }

                    assert(pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT);

                    if (pObject->IsPinned())
                    {
                        pResourceEntry = pResourceEntry->Flink;
                    }
                    else
                    {
                        EvictionList[NumObjectsToEvict++] = pObject->pUnderlying;
                        Evict(pObject);

                        pResourceEntry = ResidentObjectListHead.Flink;
                    }
                }
            }

            ManagedObject* GetResidentListHead()
            {
                if (IsListEmpty(&ResidentObjectListHead))
                {
                    return nullptr;
                }
                return CONTAINING_RECORD(ResidentObjectListHead.Flink, ManagedObject, ListEntry);
            }

            LIST_ENTRY ResidentObjectListHead;
            LIST_ENTRY EvictedObjectListHead;

            UINT32 NumResidentObjects;
            UINT32 NumEvictedObjects;

            UINT64 ResidentSize;
        };
    }

    class ResidencyManager
    {
    public:
        ResidencyManager(ImmediateContext& ImmCtx) :
            ImmCtx(ImmCtx)
        {
        }

        // NOTE: DeviceNodeIndex is an index not a mask. The majority of D3D12 uses bit masks to identify a GPU node whereas DXGI uses 0 based indices.
        HRESULT Initialize(UINT DeviceNodeIndex, IDXCoreAdapter* ParentAdapterDXCore, IDXGIAdapter3* ParentAdapterDXGI);

        void Destroy()
        {
            AsyncThreadFence.Destroy();
        }

        void BeginTrackingObject(ManagedObject* pObject)
        {
            std::lock_guard Lock(Mutex);

            if (pObject)
            {
                assert(pObject->pUnderlying != nullptr);
                LRU.Insert(pObject);
            }
        }

        void EndTrackingObject(ManagedObject* pObject)
        {
            std::lock_guard Lock(Mutex);

            LRU.Remove(pObject);
        }

        // One residency set per command-list
        HRESULT ExecuteCommandList(ID3D12CommandQueue* Queue, UINT CommandListIndex, ID3D12CommandList* CommandList, ResidencySet* pMasterSet)
        {
            return ExecuteMasterSet(Queue, CommandListIndex, pMasterSet, [Queue, CommandList]()
            {
                Queue->ExecuteCommandLists(1, &CommandList);
            });
        }

        template <typename TFunc>
        HRESULT SubmitCommandQueueCommand(ID3D12CommandQueue* Queue, UINT CommandListIndex, ResidencySet* pMasterSet, TFunc&& func)
        {
            return ExecuteMasterSet(Queue, CommandListIndex, pMasterSet, func);
        }

        HRESULT PreExecuteCommandQueueCommand(ID3D12CommandQueue* Queue, UINT CommandListIndex, ResidencySet* pMasterSet)
        {
            return PrepareToExecuteMasterSet(Queue, CommandListIndex, pMasterSet);
        }

    private:
        HRESULT PrepareToExecuteMasterSet(ID3D12CommandQueue* Queue, UINT CommandListIndex, ResidencySet* pMasterSet)
        {
            // Evict or make resident all of the objects we identified above.
            // This will run on an async thread, allowing the current to continue while still blocking the GPU if required
            // If a native async MakeResident is supported, this will run on this thread - it will only block until work referencing
            // resources which need to be evicted is completed, and does not need to wait for MakeResident to complete.
            HRESULT hr = ProcessPagingWork(CommandListIndex, pMasterSet);

            // If there are some things that need to be made resident we need to make sure that the GPU
            // doesn't execute until the async thread signals that the MakeResident call has returned.
            if (SUCCEEDED(hr))
            {
                hr = AsyncThreadFence.GPUWait(Queue);
            }
            return hr;
        }

        template <typename TFunc>
        HRESULT ExecuteMasterSet(ID3D12CommandQueue* Queue, UINT CommandListIndex, ResidencySet* pMasterSet, TFunc&& func)
        {
            HRESULT hr = S_OK;

            hr = PrepareToExecuteMasterSet(Queue, CommandListIndex, pMasterSet);

            if (SUCCEEDED(hr))
            {
                func();
            }
            return hr;
        }

        HRESULT ProcessPagingWork(UINT CommandListIndex, ResidencySet *pMasterSet);

        void GetCurrentBudget(DXCoreAdapterMemoryBudget* InfoOut, DXCoreSegmentGroup Segment)
        {
            if (AdapterDXCore)
            {
                DXCoreAdapterMemoryBudgetNodeSegmentGroup InputParams = {};
                InputParams.nodeIndex = NodeIndex;
                InputParams.segmentGroup = Segment;

                [[maybe_unused]] HRESULT hr = AdapterDXCore->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &InputParams, InfoOut);
                assert(SUCCEEDED(hr));
            }
            else
            {
                DXGI_MEMORY_SEGMENT_GROUP SegmentDXGI = (DXGI_MEMORY_SEGMENT_GROUP)Segment;
                DXGI_QUERY_VIDEO_MEMORY_INFO DXGIOut = {};
                [[maybe_unused]] HRESULT hr = AdapterDXGI->QueryVideoMemoryInfo(NodeIndex, SegmentDXGI, &DXGIOut);
                assert(SUCCEEDED(hr));

                InfoOut->availableForReservation = DXGIOut.AvailableForReservation;
                InfoOut->budget = DXGIOut.Budget;
                InfoOut->currentReservation = DXGIOut.CurrentReservation;
                InfoOut->currentUsage = DXGIOut.CurrentUsage;
            }
        }

        void WaitForSyncPoint(UINT64 FenceValues[]);

        // Generate a result between the minimum period and the maximum period based on the current
        // local memory pressure. I.e. when memory pressure is low, objects will persist longer before
        // being evicted.
        UINT64 GetCurrentEvictionGracePeriod(DXCoreAdapterMemoryBudget* LocalMemoryState)
        {
            // 1 == full pressure, 0 == no pressure
            double Pressure = (double(LocalMemoryState->currentUsage) / double(LocalMemoryState->budget));
            Pressure = std::min(Pressure, 1.0);

            if (Pressure > cTrimPercentageMemoryUsageThreshold)
            {
                // Normalize the pressure for the range 0 to cTrimPercentageMemoryUsageThreshold
                Pressure = (Pressure - cTrimPercentageMemoryUsageThreshold) / (1.0 - cTrimPercentageMemoryUsageThreshold);

                // Linearly interpolate between the min period and the max period based on the pressure
                return UINT64((MaxEvictionGracePeriodTicks - MinEvictionGracePeriodTicks) * (1.0 - Pressure)) + MinEvictionGracePeriodTicks;
            }
            else
            {
                // Essentially don't trim at all
                return MAXUINT64;
            }
        }

        ImmediateContext& ImmCtx;
        Internal::Fence AsyncThreadFence;

        CComPtr<ID3D12Device3> Device;
        // NOTE: This is an index not a mask. The majority of D3D12 uses bit masks to identify a GPU node whereas DXGI uses 0 based indices.
        UINT NodeIndex = 0;
        IDXCoreAdapter* AdapterDXCore = nullptr;
        IDXGIAdapter3* AdapterDXGI = nullptr;
        Internal::LRUCache LRU;

        std::mutex Mutex;

        static constexpr float cMinEvictionGracePeriod = 1.0f;
        UINT64 MinEvictionGracePeriodTicks;
        static constexpr float cMaxEvictionGracePeriod = 60.0f;
        UINT64 MaxEvictionGracePeriodTicks;
        // When the app is using more than this % of its budgeted local VidMem trimming will occur
        // (valid between 0.0 - 1.0)
        static constexpr float cTrimPercentageMemoryUsageThreshold = 0.7f;
    };
};
