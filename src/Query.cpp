// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{
    //==================================================================================================================================
    // Async/query/predicate/counter
    //==================================================================================================================================

    Async::Async(ImmediateContext* pDevice, EQueryType Type, UINT CommandListTypeMask) noexcept
        : DeviceChild(pDevice)
        , m_Type(Type)
        , m_CommandListTypeMask(CommandListTypeMask & pDevice->GetCommandListTypeMaskForQuery(Type))
        , m_CurrentState(AsyncState::Ended)
    {
        ZeroMemory(m_EndedCommandListID, sizeof(m_EndedCommandListID));
    }

    Async::~Async()
    {
        if (m_CurrentState == AsyncState::Begun)
        {
            // Remove this query from the list of active queries
            D3D12TranslationLayer::RemoveEntryList(&m_ActiveQueryListEntry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Async::Begin() noexcept
    {
        if (m_CurrentState == AsyncState::Begun)
        {
            End();
        }

        assert(m_CurrentState == AsyncState::Ended);

        BeginInternal(true);

        m_CurrentState = AsyncState::Begun;

        // Add this query to the list of active queries
        D3D12TranslationLayer::InsertTailList(&m_pParent->m_ActiveQueryList, &m_ActiveQueryListEntry);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool Async::RequiresBegin(EQueryType type) noexcept
    {
        switch (type)
        {
        case e_QUERY_EVENT:
        case e_QUERY_TIMESTAMP:
            return false;
        }
        return true;
    }
    bool Async::RequiresBegin() const noexcept
    {
        return RequiresBegin(m_Type);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Async::End() noexcept
    {
        // Insert a begin for those query types that require it
        if (m_CurrentState == AsyncState::Ended)
        {
            if (RequiresBegin())
            {
                Begin();

                assert(m_CurrentState == AsyncState::Begun);
            }
        }

        if (m_CurrentState == AsyncState::Begun)
        {
            // Remove this query from the list of active queries
            D3D12TranslationLayer::RemoveEntryList(&m_ActiveQueryListEntry);
        }

        EndInternal();

        m_CurrentState = AsyncState::Ended;

        ZeroMemory(m_EndedCommandListID, sizeof(m_EndedCommandListID));
        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (m_CommandListTypeMask & (1 << listType))
            {
                m_EndedCommandListID[listType] = m_pParent->GetCommandListIDWithCommands((COMMAND_LIST_TYPE)listType);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool Async::GetData(void* pData, UINT DataSize, bool DoNotFlush, bool AsyncGetData) noexcept
    {
        if (!AsyncGetData && !FlushAndPrep(DoNotFlush))
        {
            return false;
        }

        if (pData != nullptr && DataSize != 0)
        {
            GetDataInternal(pData, DataSize);
        }

        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool Async::FlushAndPrep(bool DoNotFlush) noexcept
    {
        if (m_CurrentState == AsyncState::Begun)
        {
            End();
        }

        assert(m_CurrentState == AsyncState::Ended);
        bool ret = true;
        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (m_CommandListTypeMask & (1 << listType))
            {
                COMMAND_LIST_TYPE commandListType = (COMMAND_LIST_TYPE)listType;
                if (m_EndedCommandListID[listType] == m_pParent->GetCommandListID(commandListType))
                {
                    if (DoNotFlush)
                    {
                        return false;
                    }
                    
                    // convert exceptions to bool result as method is noexcept and SubmitCommandList throws
                    try
                    {
                        m_pParent->SubmitCommandList(commandListType); // throws
                    }
                    catch (_com_error&)
                    {
                        ret = false;
                    }
                    catch (std::bad_alloc&)
                    {
                        ret = false;
                    }
                }

                UINT64 LastCompletedFence = m_pParent->GetCompletedFenceValue(commandListType);
                if (LastCompletedFence < m_EndedCommandListID[listType])
                {
                    ret = false;
                    continue;
                }
            }
        }
        return ret;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    D3D12_QUERY_TYPE Query::GetType12() const
    {
        switch (m_Type)
        {
        case e_QUERY_TIMESTAMP:
            return D3D12_QUERY_TYPE_TIMESTAMP;

        case e_QUERY_OCCLUSION:
            return D3D12_QUERY_TYPE_OCCLUSION;

        case e_QUERY_PIPELINESTATS:
            return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;

        case e_QUERY_STREAMOVERFLOWPREDICATE:
            // Addition is used to target the other streams and sum
            return D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0;

        case e_QUERY_STREAMOUTPUTSTATS: // Aliased to stream0
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM0:
            return D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0;

        case e_QUERY_STREAMOUTPUTSTATS_STREAM1:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
            return D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1;

        case e_QUERY_STREAMOUTPUTSTATS_STREAM2:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
            return D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2;

        case e_QUERY_STREAMOUTPUTSTATS_STREAM3:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
            return D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3;

            // Either Occlusion or BinaryOcclusion could be used
            // BinaryOcclusion is used for 2 reasons:
            // 1. To enable test coverage of BinaryOcclusion via wgf11 tests and 11on12
            // 2. It is more efficient on some GPUs
        case e_QUERY_OCCLUSIONPREDICATE:
            return D3D12_QUERY_TYPE_BINARY_OCCLUSION;

        case e_QUERY_VIDEO_DECODE_STATISTICS:
            return D3D12_QUERY_TYPE_VIDEO_DECODE_STATISTICS;

        default:
            assert(false);
            return static_cast<D3D12_QUERY_TYPE>(-1);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    D3D12_QUERY_HEAP_TYPE Query::GetHeapType12() const
    {
        switch (m_Type)
        {
        case e_QUERY_TIMESTAMP:
            return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

        case e_QUERY_OCCLUSIONPREDICATE:
        case e_QUERY_OCCLUSION:
            return D3D12_QUERY_HEAP_TYPE_OCCLUSION;

        case e_QUERY_PIPELINESTATS:
            return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;

        case e_QUERY_STREAMOUTPUTSTATS:
        case e_QUERY_STREAMOVERFLOWPREDICATE:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM0:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM1:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM2:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM3:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
            return D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;

        case e_QUERY_VIDEO_DECODE_STATISTICS:
            return D3D12_QUERY_HEAP_TYPE_VIDEO_DECODE_STATISTICS;

        default:
            assert(false);
            return static_cast<D3D12_QUERY_HEAP_TYPE>(-1);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    Query::~Query()
    {
        for (auto& obj : m_spQueryHeap) { AddToDeferredDeletionQueue(obj); }
        for (auto& obj : m_spResultBuffer)
        {
            if (obj.IsInitialized())
            {
                m_pParent->ReleaseSuballocatedHeap(AllocatorHeapType::Readback, obj, m_LastUsedCommandListID);
            }
        }
        for (auto& obj : m_spPredicationBuffer) { AddToDeferredDeletionQueue(obj); }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT Query::GetNumSubQueries() const
    {
        switch (m_Type)
        {
            // The D3D10 stream-output predicate is unique in that it sums
            // the results from all 4 streams
        case e_QUERY_STREAMOVERFLOWPREDICATE:
            return D3D11_SO_STREAM_COUNT;

        default:
            return 1;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::Initialize() noexcept(false)
    {
        // GetNumSubQueries() is > 1 for stream-output queries where 11on12 must accumulate the results from all 4 streams
        // m_InstancesPerQuery is a constant multiplier for all queries.  A new instance is used each time that Suspend/Resume are called
        D3D12_QUERY_HEAP_DESC QueryHeapDesc = { GetHeapType12(), GetNumSubQueries() * m_InstancesPerQuery, m_pParent->GetNodeMask() };
        UINT BufferSize = GetDataSize12() * QueryHeapDesc.Count;

        // The only query types that allows non-graphics command list type are TIMESTAMP and VIDEO_STATS for now.
        assert(m_Type == e_QUERY_TIMESTAMP || m_Type == e_QUERY_VIDEO_DECODE_STATISTICS || m_CommandListTypeMask == COMMAND_LIST_TYPE_GRAPHICS_MASK);

        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (!(m_CommandListTypeMask & (1 << listType)))
            {
                continue;
            }
            HRESULT hr = m_pParent->m_pDevice12->CreateQueryHeap(
                &QueryHeapDesc,
                IID_PPV_ARGS(&m_spQueryHeap[listType])
                );
            ThrowFailure(hr); // throw( _com_error )

            // Query data goes into a readback heap for CPU readback in GetData
            {
                m_spResultBuffer[listType] = m_pParent->AcquireSuballocatedHeap(
                    AllocatorHeapType::Readback, BufferSize, ResourceAllocationContext::FreeThread); // throw( _com_error )
            }

            // For predicates, also create a predication buffer
            {
                bool IsPredicate = false;

                switch (m_Type)
                {
                case e_QUERY_OCCLUSION:
                case e_QUERY_TIMESTAMP:
                case e_QUERY_PIPELINESTATS:
                case e_QUERY_STREAMOUTPUTSTATS:
                case e_QUERY_STREAMOUTPUTSTATS_STREAM0:
                case e_QUERY_STREAMOUTPUTSTATS_STREAM1:
                case e_QUERY_STREAMOUTPUTSTATS_STREAM2:
                case e_QUERY_STREAMOUTPUTSTATS_STREAM3:
                case e_QUERY_VIDEO_DECODE_STATISTICS:
                    IsPredicate = false;
                    break;

                    // Here D3D12 has a BOOL encoded in 64-bits
                case e_QUERY_OCCLUSIONPREDICATE:
                case e_QUERY_STREAMOVERFLOWPREDICATE:
                case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
                case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
                case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
                case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
                    IsPredicate = true;
                    break;

                default:
                    assert(false);
                }

                if (IsPredicate)
                {
                    D3D12_HEAP_PROPERTIES HeapProp = m_pParent->GetHeapProperties(
                        D3D12_HEAP_TYPE_DEFAULT
                        );

                    D3D12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
                        BufferSize,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                        );

                    // D3D12_RESOURCE_STATE_PREDICATION is the required state for SetPredication
                    hr = m_pParent->m_pDevice12->CreateCommittedResource(
                        &HeapProp,
                        D3D12_HEAP_FLAG_NONE,
                        &ResourceDesc,
                        D3D12_RESOURCE_STATE_PREDICATION,
                        nullptr,
                        IID_PPV_ARGS(&m_spPredicationBuffer[listType])
                        );
                    ThrowFailure(hr); // throw( _com_error )
                }
            }
        }
        m_CurrentInstance = 0;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::Suspend() noexcept
    {
        assert(m_CurrentInstance < m_InstancesPerQuery);

        // Store data in the query object, then resolve into the result buffer
        UINT DataSize12 = GetDataSize12();
        UINT NumSubQueries = GetNumSubQueries();
        D3D12_QUERY_TYPE QueryType12 = GetType12();

        auto DoEndQuery = [&](auto pIface, COMMAND_LIST_TYPE commandListType, UINT subQuery)
        {
            UINT Index = QueryIndex(m_CurrentInstance, subQuery, NumSubQueries);

            pIface->EndQuery(
                m_spQueryHeap[(UINT)commandListType].get(),
                static_cast<D3D12_QUERY_TYPE>(QueryType12 + subQuery),
                Index
                );

            pIface->ResolveQueryData(
                m_spQueryHeap[(UINT)commandListType].get(),
                static_cast<D3D12_QUERY_TYPE>(QueryType12 + subQuery),
                Index,
                1,
                m_spResultBuffer[(UINT)commandListType].GetResource(),
                Index * DataSize12 + m_spResultBuffer[(UINT)commandListType].GetOffset()
                );
        };

        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (m_CommandListTypeMask & (1 << listType))
            {
                COMMAND_LIST_TYPE commandListType = (COMMAND_LIST_TYPE)listType;
                m_pParent->PreRender(commandListType);
                for (UINT subquery = 0; subquery < NumSubQueries; subquery++)
                {
                    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "ImmediateContext::DiscardView must support all command list types.");

                    switch (commandListType)
                    {
                    case COMMAND_LIST_TYPE::GRAPHICS:
                        DoEndQuery(m_pParent->GetGraphicsCommandList(), commandListType, subquery);
                        break;
                    case COMMAND_LIST_TYPE::VIDEO_DECODE:
                        DoEndQuery(m_pParent->GetVideoDecodeCommandList(), commandListType, subquery);
                        break;
                    case COMMAND_LIST_TYPE::VIDEO_PROCESS:
                        DoEndQuery(m_pParent->GetVideoProcessCommandList(), commandListType, subquery);
                        break;
                    }
                }
                m_pParent->AdditionalCommandsAdded(commandListType);
                m_LastUsedCommandListID[listType] = m_pParent->GetCommandListID(commandListType);
            }
        }
    }


    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::Resume() noexcept
    {
        BeginInternal(false);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::BeginInternal(bool restart) noexcept
    {
        // These query types do not support begin
        assert(m_Type != e_QUERY_EVENT);
        assert(m_Type != e_QUERY_TIMESTAMP);
        assert(m_Type != e_QUERY_VIDEO_DECODE_STATISTICS);

        UINT NumSubQueries = GetNumSubQueries();
        D3D12_QUERY_TYPE QueryType12 = GetType12();

        static_assert(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1 == (D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0 + 1), "Adding enum");
        static_assert(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2 == (D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0 + 2), "Adding enum");
        static_assert(D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3 == (D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0 + 3), "Adding enum");

        if (restart)
        {
            // Restart at the first instance
            m_CurrentInstance = 0;
        }
        else
        {
            assert(m_CurrentInstance < m_InstancesPerQuery);

            // Increment m_CurrentInstance, accumulating if c_InstancesPerQuery would be exceeded
            AdvanceInstance();
        }

        auto DoBeginQuery = [&](auto pIface, COMMAND_LIST_TYPE commandListType, UINT subQuery)
        {
            pIface->BeginQuery(
                m_spQueryHeap[(UINT)commandListType].get(),
                static_cast<D3D12_QUERY_TYPE>(QueryType12 + subQuery),
                QueryIndex(m_CurrentInstance, subQuery, NumSubQueries)
                );
        };

        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (m_CommandListTypeMask & (1 << listType))
            {
                COMMAND_LIST_TYPE commandListType = (COMMAND_LIST_TYPE)listType;
                m_pParent->PreRender(commandListType);
                for (UINT subquery = 0; subquery < NumSubQueries; subquery++)
                {
                    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "Query::BeginInternal must support all command list types.");
                    switch (commandListType)
                    {
                    case COMMAND_LIST_TYPE::GRAPHICS:
                        DoBeginQuery(m_pParent->GetGraphicsCommandList(), commandListType, subquery);
                        break;
                    case COMMAND_LIST_TYPE::VIDEO_DECODE:
                        DoBeginQuery(m_pParent->GetVideoDecodeCommandList(), commandListType, subquery);
                        break;
                    case COMMAND_LIST_TYPE::VIDEO_PROCESS:
                        DoBeginQuery(m_pParent->GetVideoProcessCommandList(), commandListType, subquery);
                        break;
                    }
                }
                m_LastUsedCommandListID[(UINT)commandListType] = m_pParent->GetCommandListID(commandListType);
            }
        }
    }


    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::EndInternal() noexcept
    {
        // These queries do not have begin/end
        // They only have end
        // So they only ever use the first instance
        if (   e_QUERY_TIMESTAMP == m_Type
            || e_QUERY_VIDEO_DECODE_STATISTICS == m_Type)
        {
            m_CurrentInstance = 0;
        }

        assert(m_CurrentInstance < m_InstancesPerQuery);

        // This type also does not support Begin/End
        // But it should go through a different EndInternal method
        assert(e_QUERY_EVENT != m_Type);

        // Write data for current instance into the result buffer
        Suspend();

        m_CurrentInstance++;

        assert(m_CurrentInstance <= m_InstancesPerQuery);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::GetInstanceData(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize, UINT InstanceIndex) noexcept
    {
        assert(m_CurrentInstance <= m_InstancesPerQuery);

        if (m_Accumulate)
        {
            ThrowFailure(E_INVALIDARG);
        }

        if (InstanceIndex >= m_InstancesPerQuery)
        {
            ThrowFailure(E_INVALIDARG);
        }

        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (!(m_CommandListTypeMask & (1 << listType)))
            {
                continue;
            }

            void* pMappedData = nullptr;

            assert(DataSize == GetDataSize12());

            CD3DX12_RANGE ReadRange(DataSize * InstanceIndex, DataSize * (InstanceIndex + 1));
            HRESULT hr = m_spResultBuffer[listType].Map(
                0,
                &ReadRange,
                &pMappedData
                );
            ThrowFailure(hr);

            switch (m_Type)
            {
            case e_QUERY_VIDEO_DECODE_STATISTICS:
                memcpy(pData, pMappedData, DataSize);
                break;

            default:
                ThrowFailure(E_UNEXPECTED);
                break;
            }

            CD3DX12_RANGE WrittenRange(0, 0);
            m_spResultBuffer[listType].Unmap(0, &WrittenRange);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept
    {
        assert(m_CurrentInstance <= m_InstancesPerQuery);

        if (!m_Accumulate)
        {
            ThrowFailure(E_UNEXPECTED);
        }

        // initialize queries that can be used in multiple command lists
        if (m_Type == e_QUERY_TIMESTAMP)
        {
            if (DataSize < sizeof(UINT64))
            {
                ThrowFailure(E_INVALIDARG);
            }

            UINT64 *pDest = reinterpret_cast<UINT64 *>(pData);
            *pDest = 0;
        }

        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (!(m_CommandListTypeMask & (1 << listType)))
            {
                continue;
            }

            if (m_Type != e_QUERY_TIMESTAMP &&
                m_Type != e_QUERY_TIMESTAMPDISJOINT)
            {
                m_pParent->GetCommandListManager((COMMAND_LIST_TYPE)listType)->ReadbackInitiated();
            }

            void* pMappedData = nullptr;

            CD3DX12_RANGE ReadRange(0, DataSize);
            HRESULT hr = m_spResultBuffer[listType].Map(
                0,
                &ReadRange,
                &pMappedData
                );
            ThrowFailure(hr);

            UINT DataSize12 = GetDataSize12();
            UINT NumSubQueries = GetNumSubQueries();

            // All structures are arrays of 64-bit values
            assert(0 == (DataSize12 % sizeof(UINT64)));

            UINT NumCounters = DataSize12 / sizeof(UINT64);

            UINT64 TempBuffer[12];

            static_assert(sizeof(TempBuffer) >= sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS), "Temporary query buffer no large enough.");
            static_assert(sizeof(TempBuffer) >= sizeof(D3D12_QUERY_DATA_SO_STATISTICS), "Temporary query buffer no large enough.");
            assert(sizeof(TempBuffer) >= DataSize12);
            assert(_countof(TempBuffer) >= NumCounters);

            // Accumulate all instances & subqueries into a single value
            // If the query was never issued, then 0 will be returned
            ZeroMemory(TempBuffer, sizeof(TempBuffer));

            const UINT64* pSrc = reinterpret_cast<const UINT64*>(pMappedData);

            for (UINT Instance = 0; Instance < m_CurrentInstance; Instance++)
            {
                for (UINT SubQuery = 0; SubQuery < NumSubQueries; SubQuery++)
                {
                    for (UINT Counter = 0; Counter < NumCounters; Counter++)
                    {
                        TempBuffer[Counter] += pSrc[0];
                        pSrc++;
                    }
                }
            }

            switch (m_Type)
            {
                // 11 and 12 match, need to interpret & merge values from possibly multiple queues
            case e_QUERY_TIMESTAMP:
            {
                UINT64 Timestamp = (UINT64)TempBuffer[0];
                if (Timestamp > *(UINT64 *)pData)
                {
                    *(UINT64 *)pData = Timestamp;
                }
                break;
            }

            // For these types, 11 and 12 match
            case e_QUERY_OCCLUSION:
            case e_QUERY_PIPELINESTATS:
            case e_QUERY_STREAMOUTPUTSTATS:
            case e_QUERY_STREAMOUTPUTSTATS_STREAM0:
            case e_QUERY_STREAMOUTPUTSTATS_STREAM1:
            case e_QUERY_STREAMOUTPUTSTATS_STREAM2:
            case e_QUERY_STREAMOUTPUTSTATS_STREAM3:
                assert(DataSize == GetDataSize12());
                memcpy(pData, TempBuffer, DataSize);
                break;

                // Here D3D12 has a BOOL encoded in 64-bits
            case e_QUERY_OCCLUSIONPREDICATE:
            {
                __analysis_assume(DataSize == sizeof(BOOL));
                assert(DataSize == sizeof(BOOL));
                BOOL Result = (TempBuffer[0] != 0) ? TRUE : FALSE;
                memcpy(pData, &Result, sizeof(BOOL));
            }
            break;

            case e_QUERY_STREAMOVERFLOWPREDICATE:
            case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
            case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
            case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
            case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
            {
                __analysis_assume(DataSize == sizeof(BOOL));
                assert(DataSize == sizeof(BOOL));
                BOOL Result = (TempBuffer[1] != TempBuffer[0]) ? TRUE : FALSE;
                memcpy(pData, &Result, sizeof(BOOL));
            }
            break;

            default:
            {
                assert(false);
            }
            break;
            }
            CD3DX12_RANGE WrittenRange(0, 0);
            m_spResultBuffer[listType].Unmap(0, &WrittenRange);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT Query::GetDataSize12() const
    {
        // This returns the size of the data written by ResolveQueryData
        UINT Result = 0;

        switch (m_Type)
        {
        case e_QUERY_OCCLUSIONPREDICATE:
        case e_QUERY_OCCLUSION:
        case e_QUERY_TIMESTAMP:
            Result = sizeof(UINT64);
            break;

        case e_QUERY_PIPELINESTATS:
            Result = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
            break;

        case e_QUERY_STREAMOUTPUTSTATS:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM0:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM1:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM2:
        case e_QUERY_STREAMOUTPUTSTATS_STREAM3:
        case e_QUERY_STREAMOVERFLOWPREDICATE:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
            Result = sizeof(D3D12_QUERY_DATA_SO_STATISTICS);
            break;

        case e_QUERY_VIDEO_DECODE_STATISTICS:
            Result = sizeof(D3D12_QUERY_DATA_VIDEO_DECODE_STATISTICS);
            break;

        default:
            assert(false);
        }

        return Result;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void Query::AdvanceInstance()
    {
        // Used during Resume or AutoAdvance to move to the next instance
        assert(m_CurrentInstance < m_InstancesPerQuery);

        if ((m_CurrentInstance + 1) < m_InstancesPerQuery)
        {
            m_CurrentInstance++;
        }
        else if (!m_Accumulate)
        {
            // just wrap in this case and move on overwriting old entries
            m_CurrentInstance = 0;
        }
        else
        {
            // Out of instances
            // Wait for the GPU to finish all outstanding work
            m_pParent->WaitForCompletion(m_CommandListTypeMask);

            // Accumulate all results into Instance0
            void* pMappedData = nullptr;

            UINT DataSize12 = GetDataSize12();
            UINT NumSubQueries = GetNumSubQueries();

            CD3DX12_RANGE ReadRange(0, DataSize12 * NumSubQueries * m_InstancesPerQuery);
            for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
            {
                if (!(m_CommandListTypeMask & (1 << listType)))
                {
                    continue;
                }
                ThrowFailure(m_spResultBuffer[listType].Map(0, &ReadRange, &pMappedData));
                // All structures are arrays of 64-bit values
                assert(0 == (DataSize12 % sizeof(UINT64)));

                UINT NumCountersPerSubQuery = DataSize12 / sizeof(UINT64);
                UINT NumCountersPerInstance = NumCountersPerSubQuery * NumSubQueries;

                UINT64* pInstance0 = reinterpret_cast<UINT64*>(pMappedData);

                for (UINT Instance = 1; Instance <= m_CurrentInstance; Instance++)
                {
                    const UINT64* pInstance = reinterpret_cast<const UINT64*>(pMappedData) + (NumCountersPerInstance * Instance);

                    for (UINT i = 0; i < NumCountersPerInstance; i++)
                    {
                        pInstance0[i] += pInstance[i];
                    }
                }

                CD3DX12_RANGE WrittenRange(0, DataSize12 * NumSubQueries);
                m_spResultBuffer[listType].Unmap(0, &WrittenRange);
            }

            // Instance0 has valid data.  11on12 can re-use the data for instance1 and beyond
            m_CurrentInstance = 1;
        }
        assert(m_CurrentInstance < m_InstancesPerQuery);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT Query::QueryIndex(UINT Instance, UINT SubQuery, UINT NumSubQueries)
    {
        return (Instance * NumSubQueries) + SubQuery;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    // Copies data into m_spPredicationBuffer and formats it with a compute shader if necessary
    void Query::FillPredicationBuffer()
    {
#ifdef USE_PIX
        PIXSetMarker(m_pParent->GetGraphicsCommandList(), 0ull, L"Transform query data for predication");
#endif
        // This buffer is created when the query is created
        assert(m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS]);
        assert(m_CommandListTypeMask == COMMAND_LIST_TYPE_GRAPHICS_MASK);

        // Copy from the result buffer to the predication buffer
        {
            // Transition the result buffer to the CopySource state
            AutoTransition AutoTransition1(
                m_pParent->GetGraphicsCommandList(),
                m_spResultBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].GetResource(),
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_COPY_SOURCE
                );

            // Transition the predication buffer to the CopyDest state
            AutoTransition AutoTransition2(
                m_pParent->GetGraphicsCommandList(),
                m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(),
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                D3D12_RESOURCE_STATE_PREDICATION,
                D3D12_RESOURCE_STATE_COPY_DEST
                );

            UINT BufferSize = GetDataSize12() * GetNumSubQueries() * m_InstancesPerQuery;
            m_pParent->GetGraphicsCommandList()->CopyBufferRegion(
                m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(),
                0,
                m_spResultBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].GetResource(),
                m_spResultBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].GetOffset(),
                BufferSize
                );
        }

        // Run a compute shader to accumulate all instances & sub-queries.
        // Also, for stream-output queries, compare NumPrimitivesWritten to PrimitivesStorageNeeded
        // Create PSO and root signature
        m_pParent->EnsureQueryResources(); // throw( _com_error )

        // Transition the predication buffer to the UAV state
        AutoTransition AutoTransition(
            m_pParent->GetGraphicsCommandList(),
            m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(),
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_PREDICATION,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );

        UINT NumInstances = m_CurrentInstance;

        UINT NumSubQueries = GetNumSubQueries();

        UINT DataSize12 = GetDataSize12();

        // All structures are arrays of 64-bit values
        assert(0 == (DataSize12 % sizeof(UINT64)));

        UINT NumCounters = DataSize12 / sizeof(UINT64);

        {
            UINT Constants[] =
            {
                NumInstances,
                NumSubQueries * NumCounters
            };

            // This accumulates all instances down to instance 0
            m_pParent->FormatBuffer(
                m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(),
                m_pParent->m_pAccumulateQueryPSO.get(),
                0,
                (NumInstances * NumSubQueries * DataSize12) / sizeof(UINT),
                Constants
                ); // throw( _com_error )
        }

        // For stream-output, accumulate all streams and convert to BOOL (stored in 64-bits)
        bool IsStreamOut = false;

        switch (m_Type)
        {
        case e_QUERY_STREAMOVERFLOWPREDICATE:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2:
        case e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3:
            IsStreamOut = true;
            break;
        }

        if (IsStreamOut)
        {
            // UAV barrier to ensure that the calls to FormatBuffer are ordered
            m_pParent->UAVBarrier();

            UINT Constants[] =
            {
                NumSubQueries,
                0
            };

            m_pParent->FormatBuffer(
                m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(),
                m_pParent->m_pFormatQueryPSO.get(),
                0,
                (NumSubQueries * DataSize12) / sizeof(UINT),
                Constants
                ); // throw( _com_error )
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::Initialize() noexcept(false)
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::BeginInternal(bool /*restart*/) noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::EndInternal() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::Suspend() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::Resume() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void EventQuery::GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept
    {
        UNREFERENCED_PARAMETER(DataSize);
        __analysis_assume(DataSize == sizeof(BOOL));
        assert(DataSize == sizeof(BOOL));
        reinterpret_cast<BOOL*>(pData)[0] = TRUE;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::Initialize() noexcept(false)
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::BeginInternal(bool /*restart*/) noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::EndInternal() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::Suspend() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::Resume() noexcept
    {
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampDisjointQuery::GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept
    {
        UNREFERENCED_PARAMETER(DataSize);
        __analysis_assume(DataSize == sizeof(QUERY_DATA_TIMESTAMP_DISJOINT));
        assert(DataSize == sizeof(QUERY_DATA_TIMESTAMP_DISJOINT));
        QUERY_DATA_TIMESTAMP_DISJOINT* pResult = reinterpret_cast<QUERY_DATA_TIMESTAMP_DISJOINT*>(pData);

        pResult->Frequency = 0;
        UINT cLists = 0;
        for (UINT listType = 0; listType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; listType++)
        {
            if (m_CommandListTypeMask & (1 << listType))
            {
                UINT64 Frequency = 0;
                HRESULT hr = m_pParent->GetCommandQueue((COMMAND_LIST_TYPE)listType)->GetTimestampFrequency(&Frequency);
                UNREFERENCED_PARAMETER(hr);
                assert(SUCCEEDED(hr)); // this should only fail if called on the wrong queue type

                if (Frequency > pResult->Frequency)
                {
                    pResult->Frequency = Frequency;
                }
                ++cLists;
            }
        }
        pResult->Disjoint = cLists > 1;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ConditionalAutoTransition::Init(
        ID3D12GraphicsCommandList* pCommandList,
        ID3D12Resource* pResource,
        UINT Subresource,
        D3D12_RESOURCE_STATES Before,
        D3D12_RESOURCE_STATES After)
    {
        assert(m_pCommandList == nullptr && pCommandList != nullptr);
        assert(m_pResource == nullptr && pResource != nullptr);

        m_pCommandList = pCommandList;
        m_pResource = pResource;
        m_Subresource = Subresource;
        m_Before = Before;
        m_After = After;

        D3D12_RESOURCE_BARRIER BarrierDesc;
        ZeroMemory(&BarrierDesc, sizeof(BarrierDesc));

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        BarrierDesc.Transition.pResource = m_pResource;
        BarrierDesc.Transition.Subresource = m_Subresource;
        BarrierDesc.Transition.StateBefore = m_Before;
        BarrierDesc.Transition.StateAfter = m_After;

        m_pCommandList->ResourceBarrier(1, &BarrierDesc);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    ConditionalAutoTransition::~ConditionalAutoTransition()
    {
        if (m_pResource)
        {
            D3D12_RESOURCE_BARRIER BarrierDesc;
            ZeroMemory(&BarrierDesc, sizeof(BarrierDesc));

            BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            BarrierDesc.Transition.pResource = m_pResource;
            BarrierDesc.Transition.Subresource = m_Subresource;
            BarrierDesc.Transition.StateBefore = m_After;
            BarrierDesc.Transition.StateAfter = m_Before;

            m_pCommandList->ResourceBarrier(1, &BarrierDesc);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    // Runs a compute shader on a buffer UAV (R32_UINT)
    void ImmediateContext::FormatBuffer(
        ID3D12Resource* pBuffer,
        ID3D12PipelineState* pPSO,
        UINT FirstElement,
        UINT NumElements,
        const UINT Constants[NUM_UAV_ROOT_SIG_CONSTANTS]
        ) noexcept(false)
    {
        assert(m_InternalUAVRootSig.Created());

        CDisablePredication DisablePredication(this);

        // Reserve a heap slot for the UAV
        UINT ViewHeapSlot = ReserveSlots(m_ViewHeap, 1); // throw( _com_error )
        D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = m_ViewHeap.GPUHandle(ViewHeapSlot);
        D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = m_ViewHeap.CPUHandle(ViewHeapSlot);

        PreRender(COMMAND_LIST_TYPE::GRAPHICS);

        GetGraphicsCommandList()->SetPipelineState(pPSO);
        m_StatesToReassert |= e_PipelineStateDirty;

        GetGraphicsCommandList()->SetComputeRootSignature(m_InternalUAVRootSig.GetRootSignature());
        m_StatesToReassert |= e_ComputeBindingsDirty; // All bindings must be re-set after RootSig change

        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
        ZeroMemory(&UAVDesc, sizeof(UAVDesc));

        UAVDesc.Format = DXGI_FORMAT_R32_UINT;
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = FirstElement;
        UAVDesc.Buffer.NumElements = NumElements;

        m_pDevice12->CreateUnorderedAccessView(
            pBuffer,
            nullptr,
            &UAVDesc,
            CPUDescriptor
            );

        GetGraphicsCommandList()->SetComputeRootDescriptorTable(0, GPUDescriptor);

        GetGraphicsCommandList()->SetComputeRoot32BitConstants(1, NUM_UAV_ROOT_SIG_CONSTANTS, Constants, 0);

        GetGraphicsCommandList()->Dispatch(1, 1, 1);

        PostRender(COMMAND_LIST_TYPE::GRAPHICS);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TRANSLATION_API ImmediateContext::QueryBegin(Async* pAsync)
    {
        pAsync->Begin();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TRANSLATION_API ImmediateContext::QueryEnd(Async* pAsync)
    {
        pAsync->End();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool TRANSLATION_API ImmediateContext::QueryGetData(Async* pAsync, void* pData, UINT DataSize, bool DoNotFlush, bool AsyncGetData)
    {
        return pAsync->GetData(pData, DataSize, DoNotFlush, AsyncGetData);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TRANSLATION_API ImmediateContext::SetPredication(Query* pPredicate, BOOL PredicateValue)
    {
        m_CurrentState.m_pPredicate = nullptr;
        SetPredicationInternal(nullptr, FALSE);
        m_PredicateValue = PredicateValue;

        if (pPredicate)
        {
            pPredicate->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

            // Copy from the result buffer to the predication buffer
            // And format the predication buffer if necessary
            pPredicate->FillPredicationBuffer();

            m_CurrentState.m_pPredicate = pPredicate;
            SetPredicationInternal(pPredicate, PredicateValue);
        }

        m_StatesToReassert &= ~(e_PredicateDirty);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ImmediateContext::SetPredicationInternal(Query* pQuery, BOOL PredicateValue)
    {
        ID3D12Resource* pPredicationBuffer = pQuery ? pQuery->GetPredicationBuffer() : nullptr;

        GetGraphicsCommandList()->SetPredication(
            pPredicationBuffer,
            0,
            PredicateValue ? D3D12_PREDICATION_OP_NOT_EQUAL_ZERO : D3D12_PREDICATION_OP_EQUAL_ZERO
            );
    }
};