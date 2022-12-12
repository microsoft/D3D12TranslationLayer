// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    // must match COMMAND_LIST_TYPE enum
    D3D12_COMMAND_LIST_TYPE D3D12TypeMap[] =
    {
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE,
        D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS
    };

    static_assert(_countof(D3D12TypeMap) == (size_t)COMMAND_LIST_TYPE::MAX_VALID, "D3D12TypeMap must match COMMAND_LIST_TYPE enum.");

    //==================================================================================================================================
    // 
    //==================================================================================================================================

    //----------------------------------------------------------------------------------------------------------------------------------
    CommandListManager::CommandListManager(ImmediateContext *pParent, ID3D12CommandQueue *pQueue, COMMAND_LIST_TYPE type)
        : m_pParent(pParent)
        , m_type(type)
        , m_pCommandQueue(pQueue)
        , m_bNeedSubmitFence(false)
        , m_pCommandList(nullptr)
        , m_pCommandAllocator(nullptr)
        , m_AllocatorPool(false /*bLock*/, GetMaxInFlightDepth(type))
        , m_hWaitEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr)) // throw( _com_error )
        , m_MaxAllocatedUploadHeapSpacePerCommandList(cMaxAllocatedUploadHeapSpacePerCommandList)
    {
        ResetCommandListTrackingData();

        m_MaxAllocatedUploadHeapSpacePerCommandList = min(m_MaxAllocatedUploadHeapSpacePerCommandList,
            m_pParent->m_CreationArgs.MaxAllocatedUploadHeapSpacePerCommandList);
        
        if (!m_pCommandQueue)
        {
            D3D12_COMMAND_QUEUE_DESC queue = {};
            queue.Type = GetD3D12CommandListType(m_type);
            queue.NodeMask = m_pParent->GetNodeMask();
            queue.Flags = m_pParent->m_CreationArgs.DisableGPUTimeout ?
                D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT : D3D12_COMMAND_QUEUE_FLAG_NONE;
            CComPtr<ID3D12Device9> spDevice9;
            if (SUCCEEDED(m_pParent->m_pDevice12->QueryInterface(&spDevice9)))
            {
                ThrowFailure(spDevice9->CreateCommandQueue1(&queue, m_pParent->m_CreationArgs.CreatorID, IID_PPV_ARGS(&m_pCommandQueue)));
            }
            else
            {
                ThrowFailure(m_pParent->m_pDevice12->CreateCommandQueue(&queue, IID_PPV_ARGS(&m_pCommandQueue)));
            }
        }

        PrepareNewCommandList();

        m_pCommandQueue->QueryInterface(&m_pSharingContract); // Ignore failure, interface not always present.
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    CommandListManager::~CommandListManager()
    {
    }

    void CommandListManager::ReadbackInitiated() noexcept
    { 
        m_NumFlushesWithNoReadback = 0;
    }

    void CommandListManager::AdditionalCommandsAdded() noexcept
    { 
        m_NumCommands++;
    }

    void CommandListManager::DrawCommandAdded() noexcept
    {
        m_NumDraws++;
    }

    void CommandListManager::DispatchCommandAdded() noexcept
    {
        m_NumDispatches++;
    }

    void CommandListManager::UploadHeapSpaceAllocated(UINT64 heapSize) noexcept
    {
        m_UploadHeapSpaceAllocated += heapSize;
    }

    void CommandListManager::SubmitCommandListIfNeeded()
    {
        // TODO: Heuristics below haven't been heavily profiled, we'll likely want to re-visit and tune
        // this based on multiple factors when profiling (i.e. number of draws, amount of memory 
        // referenced, etc.), possibly changing on an app-by-app basis

        // These parameters attempt to avoid regressing already CPU bound applications. 
        // In these cases, submitting too frequently will make the app slower due
        // to frequently re-emitting state and the overhead of submitting/creating command lists
        static const UINT cMinDrawsOrDispatchesForSubmit = 512;
        static const UINT cMinRenderOpsForSubmit = 1000;

        // To further avoid regressing CPU bound applications, we'll stop opportunistic
        // flushing if it appears that the app doesn't need to kick off work early.
        static const UINT cMinFlushesWithNoCPUReadback = 50;

        const bool bHaveEnoughCommandsForSubmit = 
            m_NumCommands > cMinRenderOpsForSubmit ||
            m_NumDraws + m_NumDispatches > cMinDrawsOrDispatchesForSubmit;
        const bool bShouldOpportunisticFlush =
            m_NumFlushesWithNoReadback < cMinFlushesWithNoCPUReadback;
        const bool bShouldFreeUpMemory =
            m_UploadHeapSpaceAllocated > m_MaxAllocatedUploadHeapSpacePerCommandList;
        if ((bHaveEnoughCommandsForSubmit && bShouldOpportunisticFlush) ||
            bShouldFreeUpMemory)
        {
            // If the GPU is idle, submit work to keep it busy
            if (m_Fence.GetCompletedValue() == m_commandListID - 1)
            {
                if (g_hTracelogging)
                {
                    TraceLoggingWrite(g_hTracelogging,
                                      "OpportunisticFlush",
                                      TraceLoggingUInt32(m_NumCommands, "NumCommands"),
                                      TraceLoggingUInt32(m_NumDraws, "NumDraws"),
                                      TraceLoggingUInt32(m_NumDispatches, "NumDispatches"),
                                      TraceLoggingUInt64(m_UploadHeapSpaceAllocated, "UploadHeapSpaceAllocated"));
                }
                SubmitCommandListImpl();
            }
        }
    }


    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::PrepareNewCommandList()
    {
        HRESULT hr = S_OK;
        // Acquire a command allocator from the pool (or create a new one)
        auto pfnCreateNew = [](ID3D12Device* pDevice12, D3D12_COMMAND_LIST_TYPE type) -> unique_comptr<ID3D12CommandAllocator> // noexcept(false)
        {
            unique_comptr<ID3D12CommandAllocator> spAllocator;
            HRESULT hr = pDevice12->CreateCommandAllocator(
                type,
                IID_PPV_ARGS(&spAllocator)
                );
            ThrowFailure(hr); // throw( _com_error )

            return std::move(spAllocator);
        };

        auto pfnWaitForFence = [&](UINT64 fenceVal) -> bool // noexcept(false)
        {
            return WaitForFenceValue(fenceVal);
        };

        UINT64 CurrentFence = m_Fence.GetCompletedValue();

        m_pCommandAllocator = m_AllocatorPool.RetrieveFromPool(
            CurrentFence,
            pfnWaitForFence,
            pfnCreateNew,
            m_pParent->m_pDevice12.get(),
            GetD3D12CommandListType(m_type)
            );

        // Create or recycle a command list
        if (m_pCommandList)
        {
            // Recycle the previously created command list
            ResetCommandList();
        }
        else
        {
            // Create a new command list
            hr = m_pParent->m_pDevice12->CreateCommandList(m_pParent->GetNodeMask(),
                GetD3D12CommandListType(m_type),
                m_pCommandAllocator.get(),
                nullptr,
                IID_PPV_ARGS(&m_pCommandList));
        }
        ThrowFailure(hr); // throw( _com_error )

        ResetResidencySet();
        ResetCommandListTrackingData();

#if DBG
        if (m_pParent->DebugFlags() & Debug_StallExecution)
        {
            m_pCommandQueue->Wait(m_StallFence.Get(), m_commandListID);
        }
#endif
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::ResetResidencySet()
    {
        m_pResidencySet = std::make_unique<ResidencySet>();
        m_pResidencySet->Open((UINT)m_type);
    }

    HRESULT CommandListManager::PreExecuteCommandQueueCommand()
    {
        m_bNeedSubmitFence = true;
        m_pResidencySet->Close();
        return m_pParent->GetResidencyManager().PreExecuteCommandQueueCommand(m_pCommandQueue.get(), (UINT)m_type, m_pResidencySet.get());
    }

    HRESULT CommandListManager::PostExecuteCommandQueueCommand()
    {
        ResetResidencySet();
        return S_OK;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::SubmitCommandList()
    {
        ++m_NumFlushesWithNoReadback;
        SubmitCommandListImpl();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::SubmitCommandListImpl() // throws
    {
        // Walk through the list of active queries
        // and notify them that the command list is being submitted
        for (LIST_ENTRY *pListEntry = m_pParent->m_ActiveQueryList.Flink; pListEntry != &m_pParent->m_ActiveQueryList; pListEntry = pListEntry->Flink)
        {
            Async* pAsync = CONTAINING_RECORD(pListEntry, Async, m_ActiveQueryListEntry);

            pAsync->Suspend();
        }

        CloseCommandList(m_pCommandList.get()); // throws

        m_pResidencySet->Close();

        m_pParent->GetResidencyManager().ExecuteCommandList(m_pCommandQueue.get(), (UINT)m_type, m_pCommandList.get(), m_pResidencySet.get());

        // Return the command allocator to the pool for recycling
        m_AllocatorPool.ReturnToPool(std::move(m_pCommandAllocator), m_commandListID);

        SubmitFence();

        PrepareNewCommandList();

        // Walk through the list of active queries
        // and notify them that a new command list has been prepared
        for (LIST_ENTRY *pListEntry = m_pParent->m_ActiveQueryList.Flink; pListEntry != &m_pParent->m_ActiveQueryList; pListEntry = pListEntry->Flink)
        {
            Async* pAsync = CONTAINING_RECORD(pListEntry, Async, m_ActiveQueryListEntry);

            pAsync->Resume();
        }

#if DBG
        if (m_pParent->DebugFlags() & Debug_WaitOnFlush)
        {
            WaitForFenceValue(m_commandListID - 1); // throws
        }
#endif

        if (m_type == COMMAND_LIST_TYPE::GRAPHICS)
        {
            m_pParent->m_DirtyStates |= e_DirtyOnNewCommandList;
            m_pParent->m_StatesToReassert |= e_ReassertOnNewCommandList;
        }
        m_pParent->PostSubmitNotification();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::ResetCommandList()
    {
        // Reset the command allocator (indicating that the driver can recycle memory associated with it)
        ThrowFailure(m_pCommandAllocator->Reset());

        static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "CommandListManager::ResetCommandList must support all command list types.");
        switch (m_type)
        {
            case COMMAND_LIST_TYPE::GRAPHICS:
            {
                ID3D12GraphicsCommandList *pGraphicsCommandList = GetGraphicsCommandList(m_pCommandList.get());
                ThrowFailure(pGraphicsCommandList->Reset(m_pCommandAllocator.get(), nullptr));
                break;
            }

            case COMMAND_LIST_TYPE::VIDEO_DECODE:
            {
                ThrowFailure(GetVideoDecodeCommandList(m_pCommandList.get())->Reset(m_pCommandAllocator.get()));
                break;
            }

            case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            {
                ThrowFailure(GetVideoProcessCommandList(m_pCommandList.get())->Reset(m_pCommandAllocator.get()));
                break;
            }

            default:
            {
                ThrowFailure(E_UNEXPECTED);
                break;
            }
        }
        InitCommandList();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::InitCommandList()
    {
        static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "CommandListManager::InitCommandList must support all command list types.");

        switch (m_type)
        {
            case COMMAND_LIST_TYPE::GRAPHICS:
            {
                ID3D12GraphicsCommandList *pGraphicsCommandList = GetGraphicsCommandList(m_pCommandList.get());
                ID3D12DescriptorHeap* pHeaps[2] = { m_pParent->m_ViewHeap.m_pDescriptorHeap.get(), m_pParent->m_SamplerHeap.m_pDescriptorHeap.get() };
                // Sampler heap is null for compute-only devices; don't include it in the count.
                pGraphicsCommandList->SetDescriptorHeaps(m_pParent->ComputeOnly() ? 1 : 2, pHeaps);
                m_pParent->SetScissorRectsHelper();
                break;
            }

            case COMMAND_LIST_TYPE::VIDEO_DECODE:
            case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            {
                // No initialization needed
                break;
            }

            default:
            {
                ThrowFailure(E_UNEXPECTED);
                break;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::CloseCommandList(ID3D12CommandList *pCommandList)
    {
        static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "CommandListManager::CloseCommandList must support all command list types.");

        switch (m_type)
        {
            case COMMAND_LIST_TYPE::GRAPHICS:
            {
                ThrowFailure(GetGraphicsCommandList(pCommandList)->Close());
                break;
            }

            case COMMAND_LIST_TYPE::VIDEO_DECODE:
            {
                ThrowFailure(GetVideoDecodeCommandList(pCommandList)->Close());
                break;
            }

            case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            {
                ThrowFailure(GetVideoProcessCommandList(pCommandList)->Close());
                break;
            }

            default:
            {
                ThrowFailure(E_UNEXPECTED);
                break;
            }
        }

    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::AddResourceToResidencySet(Resource *pResource)
    {
        ManagedObject *pResidencyObject = pResource->GetResidencyHandle();
        if (pResidencyObject)
        {
            assert(pResidencyObject->IsInitialized());
            m_pResidencySet->Insert(pResidencyObject);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::SubmitFence() noexcept
    {
        m_pCommandQueue->Signal(m_Fence.Get(), m_commandListID);
        IncrementFence();
        m_bNeedSubmitFence = false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::IncrementFence()
    {
        InterlockedIncrement64((volatile LONGLONG*)&m_commandListID);
        UpdateLastUsedCommandListIDs();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::UpdateLastUsedCommandListIDs()
    {
        // This is required for edge cases where you have a command list that has somethings bound but with no meaningful calls (draw/dispatch),
        // and Flush is called, incrementing the command list ID. If that command list only does resource barriers, some bound objects may not
        // have their CommandListID's updated
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT64 CommandListManager::EnsureFlushedAndFenced()
    {
        m_NumFlushesWithNoReadback = 0;
        PrepForCommandQueueSync(); // throws
        UINT64 FenceValue = GetCommandListID() - 1;

        return FenceValue;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::PrepForCommandQueueSync()
    {
        if (HasCommands())
        {
            SubmitCommandListImpl(); // throws
        }
        else if (m_bNeedSubmitFence)
        {
            SubmitFence();
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    HRESULT CommandListManager::EnqueueSetEvent(HANDLE hEvent) noexcept
    {
        UINT64 FenceValue = 0;
        try {
            FenceValue = EnsureFlushedAndFenced(); // throws
        }
        catch (_com_error& e)
        {
            return e.Error();
        }
        catch (std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }

#if DBG
        if (m_pParent->DebugFlags() & Debug_StallExecution)
        {
            m_StallFence.Signal(FenceValue);
        }
#endif

        return m_Fence.SetEventOnCompletion(FenceValue, hEvent);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool CommandListManager::WaitForCompletion()
    {
        ThrowFailure(EnqueueSetEvent(m_hWaitEvent)); // throws

#ifdef USE_PIX
        PIXNotifyWakeFromFenceSignal(m_hWaitEvent);
#endif
        DWORD waitRet = WaitForSingleObject(m_hWaitEvent, INFINITE);
        UNREFERENCED_PARAMETER(waitRet);
        assert(waitRet == WAIT_OBJECT_0);
        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool CommandListManager::WaitForFenceValue(UINT64 FenceValue)
    {
        m_NumFlushesWithNoReadback = 0;

        return WaitForFenceValueInternal(true, FenceValue); // throws
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool CommandListManager::WaitForFenceValueInternal(bool IsImmediateContextThread, UINT64 FenceValue)
    {
        // Command list ID is the value of the fence that will be signaled on submission
        UINT64 CurCmdListID = IsImmediateContextThread ? m_commandListID : GetCommandListIDInterlockedRead();
        if (CurCmdListID <= FenceValue) // Using <= because value read by this thread might be stale
        {
            if (IsImmediateContextThread)
            {
                assert(CurCmdListID == FenceValue);
                if (HasCommands())
                {
                    SubmitCommandListImpl(); // throws
                }
                else
                {
                    // We submitted only an initial data command list, but no fence
                    // Just insert the fence now, and increment its value
                    assert(m_bNeedSubmitFence);
                    SubmitFence();
                }
                CurCmdListID = m_commandListID;
                assert(CurCmdListID > FenceValue);
            }
            else
            {
                return false;
            }
        }

        if (m_Fence.GetCompletedValue() >= FenceValue)
        {
            return true;
        }
        ThrowFailure(m_Fence.SetEventOnCompletion(FenceValue, m_hWaitEvent));

#if DBG
        if (m_pParent->DebugFlags() & Debug_StallExecution)
        {
            m_StallFence.Signal(FenceValue);
        }
#endif

#ifdef USE_PIX
        PIXNotifyWakeFromFenceSignal(m_hWaitEvent);
#endif
        DWORD waitRet = WaitForSingleObject(m_hWaitEvent, INFINITE);
        UNREFERENCED_PARAMETER(waitRet);
        assert(waitRet == WAIT_OBJECT_0);
        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CommandListManager::DiscardCommandList()
    {
        ResetCommandListTrackingData();
        m_pCommandList = nullptr;

        m_pResidencySet->Close();
    }

    D3D12_COMMAND_LIST_TYPE CommandListManager::GetD3D12CommandListType(COMMAND_LIST_TYPE type)
    {
        if (ComputeOnly())
        {
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        }
        else
        {
            return D3D12TypeMap[(UINT)type];
        }
    }

}
