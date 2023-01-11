// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{

void Internal::LRUCache::TrimToSyncPointInclusive(INT64 CurrentUsage, INT64 CurrentBudget, std::vector<ID3D12Pageable*> &EvictionList, UINT64 FenceValues[])
{
    EvictionList.clear();
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
            EvictionList.push_back(pObject->pUnderlying);
            Evict(pObject);

            CurrentUsage -= pObject->Size;

            pResourceEntry = ResidentObjectListHead.Flink;
        }
    }
}

void Internal::LRUCache::TrimAgedAllocations(UINT64 FenceValues[], std::vector<ID3D12Pageable*> &EvictionList, UINT64 CurrentTimeStamp, UINT64 MinDelta)
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
            EvictionList.push_back(pObject->pUnderlying);
            Evict(pObject);

            pResourceEntry = ResidentObjectListHead.Flink;
        }
    }
}

HRESULT ResidencyManager::Initialize(UINT DeviceNodeIndex, IDXCoreAdapter *ParentAdapterDXCore, IDXGIAdapter3 *ParentAdapterDXGI)
{
    NodeIndex = DeviceNodeIndex;
    AdapterDXCore = ParentAdapterDXCore;
    AdapterDXGI = ParentAdapterDXGI;

    if (FAILED(ImmCtx.m_pDevice12->QueryInterface(&Device)))
    {
        return E_NOINTERFACE;
    }

    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    // Calculate how many QPC ticks are equivalent to the given time in seconds
    MinEvictionGracePeriodTicks = UINT64(Frequency.QuadPart * cMinEvictionGracePeriod);
    MaxEvictionGracePeriodTicks = UINT64(Frequency.QuadPart * cMaxEvictionGracePeriod);
    BudgetQueryPeriodTicks = UINT64(Frequency.QuadPart * cBudgetQueryPeriod);

    HRESULT hr = S_OK;
    hr = AsyncThreadFence.Initialize(Device);

    return hr;
}

HRESULT ResidencyManager::ProcessPagingWork(UINT CommandListIndex, ResidencySet *pMasterSet)
{
    // the size of all the objects which will need to be made resident in order to execute this set.
    UINT64 SizeToMakeResident = 0;

    LARGE_INTEGER CurrentTime;
    QueryPerformanceCounter(&CurrentTime);

    HRESULT hr = S_OK;
    {
        // A lock must be taken here as the state of the objects will be altered
        std::lock_guard Lock(Mutex);

        MakeResidentList.reserve(pMasterSet->Set.size());
        EvictionList.reserve(LRU.NumResidentObjects);

        // Mark the objects used by this command list to be made resident
        for (auto pObject : pMasterSet->Set)
        {
            // If it's evicted we need to make it resident again
            if (pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::EVICTED)
            {
                MakeResidentList.push_back({ pObject });
                LRU.MakeResident(pObject);

                SizeToMakeResident += pObject->Size;
            }

            // Update the last sync point that this was used on
            // Note: This can be used for app command queues as well, but in that case, they'll
            // be pinned rather than relying on this implicit sync point tracking.
            if (CommandListIndex < (UINT)COMMAND_LIST_TYPE::MAX_VALID)
            {
                pObject->LastUsedFenceValues[CommandListIndex] = ImmCtx.GetCommandListID((COMMAND_LIST_TYPE)CommandListIndex);
            }

            pObject->LastUsedTimestamp = CurrentTime.QuadPart;
            LRU.ObjectReferenced(pObject);
        }

        DXCoreAdapterMemoryBudget LocalMemory;
        ZeroMemory(&LocalMemory, sizeof(LocalMemory));
        GetCurrentBudget(CurrentTime.QuadPart, &LocalMemory);

        UINT64 EvictionGracePeriod = GetCurrentEvictionGracePeriod(&LocalMemory);
        UINT64 LastSubmittedFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT64 WaitedFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            LastSubmittedFenceValues[i] = ImmCtx.GetCommandListID((COMMAND_LIST_TYPE)i) - 1;
            WaitedFenceValues[i] = ImmCtx.GetCompletedFenceValue((COMMAND_LIST_TYPE)i);
        }
        LRU.TrimAgedAllocations(WaitedFenceValues, EvictionList, CurrentTime.QuadPart, EvictionGracePeriod);

        if (!EvictionList.empty())
        {
            [[maybe_unused]] HRESULT hrEvict = Device->Evict((UINT)EvictionList.size(), EvictionList.data());
            assert(SUCCEEDED(hrEvict));
            EvictionList.clear();
        }

        if (!MakeResidentList.empty())
        {
            UINT32 ObjectsMadeResident = 0;
            UINT32 MakeResidentIndex = 0;
            while (true)
            {
                INT64 TotalUsage = LocalMemory.currentUsage;
                INT64 TotalBudget = LocalMemory.budget;

                INT64 AvailableSpace = TotalBudget - TotalUsage;

                UINT64 BatchSize = 0;
                UINT32 NumObjectsInBatch = 0;
                UINT32 BatchStart = MakeResidentIndex;

                if (AvailableSpace > 0)
                {
                    for (UINT32 i = MakeResidentIndex; i < MakeResidentList.size(); i++)
                    {
                        // If we try to make this object resident, will we go over budget?
                        if (BatchSize + MakeResidentList[i].pManagedObject->Size > UINT64(AvailableSpace))
                        {
                            // Next time we will start here
                            MakeResidentIndex = i;
                            break;
                        }
                        else
                        {
                            BatchSize += MakeResidentList[i].pManagedObject->Size;
                            NumObjectsInBatch++;
                            ObjectsMadeResident++;

                            MakeResidentList[i].pUnderlying = MakeResidentList[i].pManagedObject->pUnderlying;
                        }
                    }

                    hr = Device->EnqueueMakeResident(D3D12_RESIDENCY_FLAG_NONE,
                                                     NumObjectsInBatch,
                                                     &MakeResidentList[BatchStart].pUnderlying,
                                                     AsyncThreadFence.pFence,
                                                     AsyncThreadFence.FenceValue + 1);
                    if (SUCCEEDED(hr))
                    {
                        AsyncThreadFence.Increment();
                        SizeToMakeResident -= BatchSize;
                    }
                }

                if (FAILED(hr) || ObjectsMadeResident != MakeResidentList.size())
                {
                    ManagedObject *pResidentHead = LRU.GetResidentListHead();
                    while (pResidentHead && pResidentHead->IsPinned())
                    {
                        pResidentHead = CONTAINING_RECORD(pResidentHead->ListEntry.Flink, ManagedObject, ListEntry);
                    }

                    // If there is nothing to trim OR the only objects 'Resident' are the ones about to be used by this execute.
                    bool ForceResidency = pResidentHead == nullptr;
                    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID && !ForceResidency; ++i)
                    {
                        ForceResidency = pResidentHead->LastUsedFenceValues[i] > LastSubmittedFenceValues[i];
                    }
                    if (ForceResidency)
                    {
                        // Make resident the rest of the objects as there is nothing left to trim
                        UINT32 NumObjects = (UINT32)MakeResidentList.size() - ObjectsMadeResident;

                        // Gather up the remaining underlying objects
                        for (UINT32 i = MakeResidentIndex; i < MakeResidentList.size(); i++)
                        {
                            MakeResidentList[i].pUnderlying = MakeResidentList[i].pManagedObject->pUnderlying;
                        }

                        hr = Device->EnqueueMakeResident(D3D12_RESIDENCY_FLAG_NONE,
                                                         NumObjects,
                                                         &MakeResidentList[MakeResidentIndex].pUnderlying,
                                                         AsyncThreadFence.pFence,
                                                         AsyncThreadFence.FenceValue + 1);
                        if (SUCCEEDED(hr))
                        {
                            AsyncThreadFence.Increment();
                        }
                        if (FAILED(hr))
                        {
                            // TODO: What should we do if this fails? This is a catastrophic failure in which the app is trying to use more memory
                            //       in 1 command list than can possibly be made resident by the system.
                            assert(SUCCEEDED(hr));
                        }
                        break;
                    }

                    // Wait until the GPU is done
                    UINT64 *FenceValuesToWaitFor = pResidentHead ? pResidentHead->LastUsedFenceValues : LastSubmittedFenceValues;
                    WaitForSyncPoint(FenceValuesToWaitFor);
                    std::copy(FenceValuesToWaitFor, FenceValuesToWaitFor + (UINT)COMMAND_LIST_TYPE::MAX_VALID, WaitedFenceValues);

                    LRU.TrimToSyncPointInclusive(TotalUsage + INT64(SizeToMakeResident), TotalBudget, EvictionList, WaitedFenceValues);

                    [[maybe_unused]] HRESULT hrEvict = Device->Evict((UINT)EvictionList.size(), EvictionList.data());
                    assert(SUCCEEDED(hrEvict));
                }
                else
                {
                    // We made everything resident, mission accomplished
                    break;
                }
            }
        }

        MakeResidentList.clear();
        EvictionList.clear();
        return hr;
    }
}

static void GetDXCoreBudget(IDXCoreAdapter *AdapterDXCore, UINT NodeIndex, DXCoreAdapterMemoryBudget *InfoOut, DXCoreSegmentGroup Segment)
{
    DXCoreAdapterMemoryBudgetNodeSegmentGroup InputParams = {};
    InputParams.nodeIndex = NodeIndex;
    InputParams.segmentGroup = Segment;

    [[maybe_unused]] HRESULT hr = AdapterDXCore->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &InputParams, InfoOut);
    assert(SUCCEEDED(hr));
}
static void GetDXGIBudget(IDXGIAdapter3 *AdapterDXGI, UINT NodeIndex, DXGI_QUERY_VIDEO_MEMORY_INFO *InfoOut, DXGI_MEMORY_SEGMENT_GROUP Segment)
{
    [[maybe_unused]] HRESULT hr = AdapterDXGI->QueryVideoMemoryInfo(NodeIndex, Segment, InfoOut);
    assert(SUCCEEDED(hr));
}

void ResidencyManager::GetCurrentBudget(UINT64 Timestamp, DXCoreAdapterMemoryBudget* InfoOut)
{
    if (Timestamp - LastBudgetTimestamp >= BudgetQueryPeriodTicks)
    {
        LastBudgetTimestamp = Timestamp;
        if (AdapterDXCore)
        {
            DXCoreAdapterMemoryBudget Local, Nonlocal;
            GetDXCoreBudget(AdapterDXCore, NodeIndex, &Local, DXCoreSegmentGroup::Local);
            GetDXCoreBudget(AdapterDXCore, NodeIndex, &Nonlocal, DXCoreSegmentGroup::NonLocal);
            CachedBudget.currentUsage = Local.currentUsage + Nonlocal.currentUsage;
            CachedBudget.budget = Local.budget + Nonlocal.budget;
        }
        else
        {
            DXGI_QUERY_VIDEO_MEMORY_INFO Local, Nonlocal;
            GetDXGIBudget(AdapterDXGI, NodeIndex, &Local, DXGI_MEMORY_SEGMENT_GROUP_LOCAL);
            GetDXGIBudget(AdapterDXGI, NodeIndex, &Nonlocal, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL);
            CachedBudget.currentUsage = Local.CurrentUsage + Nonlocal.CurrentUsage;
            CachedBudget.budget = Local.Budget + Nonlocal.Budget;
        }
    }
    *InfoOut = CachedBudget;
}

void ResidencyManager::WaitForSyncPoint(UINT64 FenceValues[])
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
    {
        ImmCtx.WaitForFenceValue((COMMAND_LIST_TYPE)i, FenceValues[i]);
    }
}
}
