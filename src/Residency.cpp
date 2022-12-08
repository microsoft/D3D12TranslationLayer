// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{
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

    HRESULT hr = S_OK;
    hr = AsyncThreadFence.Initialize(Device);

    return hr;
}

HRESULT ResidencyManager::ProcessPagingWork(UINT CommandListIndex, ResidencySet *pMasterSet)
{
    // Use a union so that we only need 1 allocation
    union ResidentScratchSpace
    {
        ManagedObject *pManagedObject;
        ID3D12Pageable *pUnderlying;
    };

    ResidentScratchSpace *pMakeResidentList = nullptr;
    UINT32 NumObjectsToMakeResident = 0;

    ID3D12Pageable **pEvictionList = nullptr;
    UINT32 NumObjectsToEvict = 0;

    // the size of all the objects which will need to be made resident in order to execute this set.
    UINT64 SizeToMakeResident = 0;

    LARGE_INTEGER CurrentTime;
    QueryPerformanceCounter(&CurrentTime);

    HRESULT hr = S_OK;
    {
        // A lock must be taken here as the state of the objects will be altered
        std::lock_guard Lock(Mutex);

        pMakeResidentList = new ResidentScratchSpace[pMasterSet->Set.size()];
        pEvictionList = new ID3D12Pageable * [LRU.NumResidentObjects];

        // Mark the objects used by this command list to be made resident
        for (auto pObject : pMasterSet->Set)
        {
            // If it's evicted we need to make it resident again
            if (pObject->ResidencyStatus == ManagedObject::RESIDENCY_STATUS::EVICTED)
            {
                pMakeResidentList[NumObjectsToMakeResident++].pManagedObject = pObject;
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
        GetCurrentBudget(&LocalMemory, DXCoreSegmentGroup::Local);

        UINT64 EvictionGracePeriod = GetCurrentEvictionGracePeriod(&LocalMemory);
        UINT64 LastSubmittedFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT64 WaitedFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            LastSubmittedFenceValues[i] = ImmCtx.GetCommandListID((COMMAND_LIST_TYPE)i) - 1;
            WaitedFenceValues[i] = ImmCtx.GetCompletedFenceValue((COMMAND_LIST_TYPE)i);
        }
        LRU.TrimAgedAllocations(WaitedFenceValues, pEvictionList, NumObjectsToEvict, CurrentTime.QuadPart, EvictionGracePeriod);

        if (NumObjectsToEvict)
        {
            [[maybe_unused]] HRESULT hrEvict = Device->Evict(NumObjectsToEvict, pEvictionList);
            assert(SUCCEEDED(hrEvict));
            NumObjectsToEvict = 0;
        }

        if (NumObjectsToMakeResident)
        {
            UINT32 ObjectsMadeResident = 0;
            UINT32 MakeResidentIndex = 0;
            while (true)
            {
                ZeroMemory(&LocalMemory, sizeof(LocalMemory));

                GetCurrentBudget(&LocalMemory, DXCoreSegmentGroup::Local);
                DXCoreAdapterMemoryBudget NonLocalMemory;
                ZeroMemory(&NonLocalMemory, sizeof(NonLocalMemory));
                GetCurrentBudget(&NonLocalMemory, DXCoreSegmentGroup::NonLocal);

                INT64 TotalUsage = LocalMemory.currentUsage + NonLocalMemory.currentUsage;
                INT64 TotalBudget = LocalMemory.budget + NonLocalMemory.budget;

                INT64 AvailableSpace = TotalBudget - TotalUsage;

                UINT64 BatchSize = 0;
                UINT32 NumObjectsInBatch = 0;
                UINT32 BatchStart = MakeResidentIndex;

                if (AvailableSpace > 0)
                {
                    for (UINT32 i = MakeResidentIndex; i < NumObjectsToMakeResident; i++)
                    {
                        // If we try to make this object resident, will we go over budget?
                        if (BatchSize + pMakeResidentList[i].pManagedObject->Size > UINT64(AvailableSpace))
                        {
                            // Next time we will start here
                            MakeResidentIndex = i;
                            break;
                        }
                        else
                        {
                            BatchSize += pMakeResidentList[i].pManagedObject->Size;
                            NumObjectsInBatch++;
                            ObjectsMadeResident++;

                            pMakeResidentList[i].pUnderlying = pMakeResidentList[i].pManagedObject->pUnderlying;
                        }
                    }

                    hr = Device->EnqueueMakeResident(D3D12_RESIDENCY_FLAG_NONE,
                                                     NumObjectsInBatch,
                                                     &pMakeResidentList[BatchStart].pUnderlying,
                                                     AsyncThreadFence.pFence,
                                                     AsyncThreadFence.FenceValue + 1);
                    if (SUCCEEDED(hr))
                    {
                        AsyncThreadFence.Increment();
                        SizeToMakeResident -= BatchSize;
                    }
                }

                if (FAILED(hr) || ObjectsMadeResident != NumObjectsToMakeResident)
                {
                    ManagedObject *pResidentHead = LRU.GetResidentListHead();
                    while (pResidentHead && pResidentHead->IsPinned())
                    {
                        pResidentHead = CONTAINING_RECORD(pResidentHead->ListEntry.Flink, ManagedObject, ListEntry);
                    }

                    // If there is nothing to trim OR the only objects 'Resident' are the ones about to be used by this execute.
                    if (pResidentHead == nullptr ||
                        std::equal(WaitedFenceValues, std::end(WaitedFenceValues), LastSubmittedFenceValues))
                    {
                        // Make resident the rest of the objects as there is nothing left to trim
                        UINT32 NumObjects = NumObjectsToMakeResident - ObjectsMadeResident;

                        // Gather up the remaining underlying objects
                        for (UINT32 i = MakeResidentIndex; i < NumObjectsToMakeResident; i++)
                        {
                            pMakeResidentList[i].pUnderlying = pMakeResidentList[i].pManagedObject->pUnderlying;
                        }

                        hr = Device->EnqueueMakeResident(D3D12_RESIDENCY_FLAG_NONE,
                                                         NumObjects,
                                                         &pMakeResidentList[MakeResidentIndex].pUnderlying,
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

                    LRU.TrimToSyncPointInclusive(TotalUsage + INT64(SizeToMakeResident), TotalBudget, pEvictionList, NumObjectsToEvict, WaitedFenceValues);

                    [[maybe_unused]] HRESULT hrEvict = Device->Evict(NumObjectsToEvict, pEvictionList);
                    assert(SUCCEEDED(hrEvict));
                } else
                {
                    // We made everything resident, mission accomplished
                    break;
                }
            }
        }

        delete[](pMakeResidentList);
        delete[](pEvictionList);
        return hr;
    }
}

void ResidencyManager::WaitForSyncPoint(UINT64 FenceValues[])
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
    {
        ImmCtx.WaitForFenceValue((COMMAND_LIST_TYPE)i, FenceValues[i]);
    }
}
}
