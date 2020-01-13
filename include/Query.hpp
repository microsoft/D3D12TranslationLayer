// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class ConditionalAutoTransition
    {
    public:
        ConditionalAutoTransition() : m_pCommandList(nullptr), m_pResource(nullptr) {}
        void Init(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, UINT Subresource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After);
        
        ~ConditionalAutoTransition();
    private:
        ID3D12GraphicsCommandList* m_pCommandList;
        ID3D12Resource* m_pResource;
        UINT m_Subresource;
        D3D12_RESOURCE_STATES m_Before;
        D3D12_RESOURCE_STATES m_After;
    };

    class AutoTransition : public ConditionalAutoTransition
    {
    public:
        AutoTransition(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, UINT Subresource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After)
        {
            Init(pCommandList, pResource, Subresource, Before, After);
        }
    };


    //==================================================================================================================================
    // Async
    // Stores data responsible for remapping D3D11 async (e.g. queries) to underlying D3D12 async
    //==================================================================================================================================

    enum EQueryType
    {
        e_QUERY_EVENT = 0,
        e_QUERY_OCCLUSION,
        e_QUERY_TIMESTAMP,
        e_QUERY_TIMESTAMPDISJOINT,
        e_QUERY_PIPELINESTATS,
        e_QUERY_OCCLUSIONPREDICATE,
        e_QUERY_STREAMOUTPUTSTATS,
        e_QUERY_STREAMOVERFLOWPREDICATE,
        e_QUERY_STREAMOUTPUTSTATS_STREAM0,
        e_QUERY_STREAMOUTPUTSTATS_STREAM1,
        e_QUERY_STREAMOUTPUTSTATS_STREAM2,
        e_QUERY_STREAMOUTPUTSTATS_STREAM3,
        e_QUERY_STREAMOVERFLOWPREDICATE_STREAM0,
        e_QUERY_STREAMOVERFLOWPREDICATE_STREAM1,
        e_QUERY_STREAMOVERFLOWPREDICATE_STREAM2,
        e_QUERY_STREAMOVERFLOWPREDICATE_STREAM3,
        e_QUERY_VIDEO_DECODE_STATISTICS,
        e_COUNTER_GPU_IDLE = 0x1000, // Start of "counters"
        e_COUNTER_VERTEX_PROCESSING,
        e_COUNTER_GEOMETRY_PROCESSING,
        e_COUNTER_PIXEL_PROCESSING,
        e_COUNTER_OTHER_GPU_PROCESSING,
        e_COUNTER_HOST_ADAPTER_BANDWIDTH_UTILIZATION,
        e_COUNTER_LOCAL_VIDMEM_BANDWIDTH_UTILIZATION,
        e_COUNTER_VERTEX_THROUGHPUT_UTILIZATION,
        e_COUNTER_TRISETUP_THROUGHPUT_UTILIZATION,
        e_COUNTER_FILLRATE_THROUGHPUT_UTILIZATION,
        e_COUNTER_VERTEXSHADER_MEMORY_LIMITED,
        e_COUNTER_VERTEXSHADER_COMPUTATION_LIMITED,
        e_COUNTER_GEOMETRYSHADER_MEMORY_LIMITED,
        e_COUNTER_GEOMETRYSHADER_COMPUTATION_LIMITED,
        e_COUNTER_PIXELSHADER_MEMORY_LIMITED,
        e_COUNTER_PIXELSHADER_COMPUTATION_LIMITED,
        e_COUNTER_POST_TRANSFORM_CACHE_HIT_RATE,
        e_COUNTER_TEXTURE_CACHE_HIT_RATE,
    };

    class Async : public DeviceChild
    {
    public:
        enum class AsyncState { Begun, Ended };

    public:
        Async(ImmediateContext* pDevice, EQueryType Type, UINT CommandListTypeMask) noexcept;
        virtual ~Async() noexcept;

        virtual void Initialize() noexcept(false) = 0;

        virtual void Suspend() noexcept = 0;
        virtual void Resume() noexcept = 0;

        void Begin() noexcept;
        void End() noexcept;
        bool GetData(void* pData, UINT DataSize, bool DoNotFlush, bool AsyncGetData) noexcept;

        bool FlushAndPrep(bool DoNotFlush) noexcept;

        static bool RequiresBegin(EQueryType type) noexcept;
        bool RequiresBegin() const noexcept;

    protected:
        virtual void BeginInternal(bool restart) noexcept = 0;
        virtual void EndInternal() noexcept = 0;
        virtual void GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept = 0;

    public:
        LIST_ENTRY m_ActiveQueryListEntry;
        EQueryType m_Type;
        AsyncState m_CurrentState;
        UINT64 m_EndedCommandListID[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT m_CommandListTypeMask;
    };

    class Query : public Async
    {
    public:
        Query(ImmediateContext* pDevice, EQueryType Type, UINT CommandListTypeMask, UINT nInstances = c_DefaultInstancesPerQuery) noexcept
            : Async(pDevice, Type, CommandListTypeMask)
            , m_CurrentInstance(0)
            , m_InstancesPerQuery(nInstances)
            , m_Accumulate(Type != e_QUERY_VIDEO_DECODE_STATISTICS)
        { }
        virtual ~Query();

        virtual void Initialize() noexcept(false);
        virtual void Suspend() noexcept;
        virtual void Resume() noexcept;

        void FillPredicationBuffer();
        ID3D12Resource *GetPredicationBuffer() { return m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::GRAPHICS].get(); }
        void GetInstanceData(void* pData, UINT DataSize, UINT InstanceIndex) noexcept;
        UINT GetCurrentInstance() { return m_CurrentInstance;  }

    protected:
        virtual void BeginInternal(bool restart) noexcept;
        virtual void EndInternal() noexcept;
        virtual void GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept;

        D3D12_QUERY_TYPE GetType12() const;
        D3D12_QUERY_HEAP_TYPE GetHeapType12() const;
        UINT GetNumSubQueries() const;
        UINT GetDataSize12() const;
        void AdvanceInstance();
        UINT QueryIndex(UINT Instance, UINT SubQuery, UINT NumSubQueries);

        static const UINT c_DefaultInstancesPerQuery = 4;

    protected:
        unique_comptr<ID3D12QueryHeap> m_spQueryHeap[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        D3D12ResourceSuballocation m_spResultBuffer[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        unique_comptr<ID3D12Resource> m_spPredicationBuffer[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT m_CurrentInstance;
        const bool m_Accumulate;
        const UINT m_InstancesPerQuery;
    };

    class EventQuery : public Async
    {
    public:
        EventQuery(ImmediateContext* pDevice, UINT CommandListTypeMask) noexcept
            : Async(pDevice, e_QUERY_EVENT, CommandListTypeMask) { }

        virtual void Initialize() noexcept(false);
        virtual void Suspend() noexcept;
        virtual void Resume() noexcept;

    protected:
        virtual void BeginInternal(bool restart) noexcept;
        virtual void EndInternal() noexcept;
        virtual void GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept;
    };

    struct QUERY_DATA_TIMESTAMP_DISJOINT
    {
        UINT64 Frequency;
        BOOL Disjoint;
    };

    class TimestampDisjointQuery : public Async
    {
    public:
        TimestampDisjointQuery(ImmediateContext* pDevice, UINT CommandListTypeMask) noexcept
            : Async(pDevice, e_QUERY_TIMESTAMPDISJOINT, CommandListTypeMask) { }

        virtual void Initialize() noexcept(false);
        virtual void Suspend() noexcept;
        virtual void Resume() noexcept;

    protected:
        virtual void BeginInternal(bool restart) noexcept;
        virtual void EndInternal() noexcept;
        virtual void GetDataInternal(_Out_writes_bytes_(DataSize) void* pData, UINT DataSize) noexcept;
    };
};