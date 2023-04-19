// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{

class BatchedQuery;

struct BatchedExtension
{
    virtual void Dispatch(ImmediateContext& ImmCtx, const void* pData, size_t DataSize) = 0;
};

class FreePageContainer
{
    OptLock<> m_CS;
    void* m_FreePageHead = nullptr;

public:
    class LockedAdder
    {
        std::unique_lock<std::mutex> m_Lock;
        void*& m_FreePageHead;

    public:
        void AddPage(void* pPage) noexcept;
        LockedAdder(FreePageContainer& Container) noexcept
            : m_Lock(Container.m_CS.TakeLock())
            , m_FreePageHead(Container.m_FreePageHead)
        {
        }
    };
    FreePageContainer(bool bAllocCS) : m_CS(bAllocCS) {}
    ~FreePageContainer();
    void* RemovePage() noexcept;
};

class BatchedContext
{
private:
// Gives a 0-indexed value to each command in the order they're declared.
#define DECLARE_COMMAND_VALUE static constexpr UINT CmdValue = (__COUNTER__ - c_FirstCommandCounterValue);

public:
    static constexpr size_t BatchSizeInBytes = 2048;
    struct BatchStorageAllocator
    {
        FreePageContainer* m_Container;
        void* operator()(bool bAllocSuccess) noexcept;
    };
    using BatchPrimitive = UINT64;
    using BatchStorage = segmented_stack<BatchPrimitive, BatchSizeInBytes / sizeof(BatchPrimitive), BatchStorageAllocator>;
    static constexpr UINT c_MaxOutstandingBatches = 5;

    class Batch
    {
        friend class BatchedContext;

        uint64_t m_BatchID;

        BatchStorage m_BatchCommands;
        std::vector<std::function<void()>> m_PostBatchFunctions;
        UINT m_NumCommands;

        // Used to check GPU completion. Guarded by submission lock.
        UINT m_FlushRequestedMask = 0;

        Batch(BatchStorageAllocator const& allocator)
            : m_BatchCommands(std::nothrow, allocator)
        {
        }
        void Retire(FreePageContainer& FreePages) noexcept;
        void PrepareToSubmit(BatchStorage BatchCommands, std::vector<std::function<void()>> PostBatchFunctions, uint64_t BatchID, UINT NumCommands, bool bFlushImmCtxAfterBatch);
    };

    static const void* AlignPtr(const void* pPtr) noexcept
    {
        return reinterpret_cast<const void*>(Align(reinterpret_cast<size_t>(pPtr), sizeof(BatchPrimitive)));
    }
    static void* AlignPtr(void* pPtr) noexcept
    {
        return reinterpret_cast<void*>(Align(reinterpret_cast<size_t>(pPtr), sizeof(BatchPrimitive)));
    }

    // Note: This value could be anything, depending on how many times __COUNTER__ was instantiated.
    // But all further references of __COUNTER__ in this file subtract this value, thereby becoming
    // 0-based.
    static constexpr UINT c_FirstCommandCounterValue = __COUNTER__ + 1;
    struct CmdSetPipelineState
    {
        DECLARE_COMMAND_VALUE;
        PipelineState* pPSO;
    };
    struct CmdDrawInstanced
    {
        DECLARE_COMMAND_VALUE;
        UINT countPerInstance;
        UINT instanceCount;
        UINT vertexStart;
        UINT instanceStart;
    };
    struct CmdDrawIndexedInstanced
    {
        DECLARE_COMMAND_VALUE;
        UINT countPerInstance;
        UINT instanceCount;
        UINT indexStart;
        INT vertexStart;
        UINT instanceStart;
    };
    struct CmdDispatch
    {
        DECLARE_COMMAND_VALUE;
        UINT x, y, z;
    };
    struct CmdDrawAuto
    {
        DECLARE_COMMAND_VALUE;
    };
    struct CmdDrawInstancedIndirect
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBuffer;
        UINT offset;
    };
    struct CmdDrawIndexedInstancedIndirect
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBuffer;
        UINT offset;
    };
    struct CmdDispatchIndirect
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBuffer;
        UINT offset;
    };
    struct CmdSetTopology
    {
        DECLARE_COMMAND_VALUE;
        D3D12_PRIMITIVE_TOPOLOGY topology;
    };
    struct CmdSetVertexBuffers
    {
        DECLARE_COMMAND_VALUE;
        UINT _CmdValue = CmdValue;
        UINT startSlot;
        UINT numVBs;
        CmdSetVertexBuffers(UINT _startSlot, UINT _numVBs, Resource* const* _ppVBs, UINT const* _pStrides, UINT const* _pOffsets);
        static size_t GetCommandSize(UINT, UINT _numVBs, Resource* const*, UINT const*, UINT const*);
        // Followed by Resource[numVBs], then UINT[numVBs], then UINT[numVBs]
    };
    struct CmdSetIndexBuffer
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBuffer;
        DXGI_FORMAT format;
        UINT offset;
    };
    struct CmdSetShaderResources
    {
        DECLARE_COMMAND_VALUE;
        EShaderStage stage;
        UINT startSlot;
        UINT numSRVs;
        // Followed by SRV[numSRVs]
    };
    struct CmdSetSamplers
    {
        DECLARE_COMMAND_VALUE;
        EShaderStage stage;
        UINT startSlot;
        UINT numSamplers;
        // Followed by Sampler[numSamplers]
    };
    struct CmdSetConstantBuffers
    {
        DECLARE_COMMAND_VALUE;
        UINT _CmdValue = CmdValue;
        EShaderStage stage;
        UINT startSlot;
        UINT numCBs;
        CmdSetConstantBuffers(EShaderStage _stage, UINT _startSlot, UINT _numCBs, Resource* const* _ppCBs, UINT const* _pFirstConstant, UINT const* _pNumConstants);
        static size_t GetCommandSize(EShaderStage, UINT, UINT numCBs, Resource* const*, UINT const*, UINT const*);
        // Followed by Resource[numCBs], then UINT[numCBs], then UINT[numCBs]
    };
    struct CmdSetConstantBuffersNullOffsetSize
    {
        DECLARE_COMMAND_VALUE;
        EShaderStage stage;
        UINT startSlot;
        UINT numCBs;
        // Followed by Resource[numCBs]
    };
    struct CmdSetSOBuffers
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBuffers[D3D11_SO_STREAM_COUNT];
        UINT offsets[D3D11_SO_STREAM_COUNT];
    };
    struct CmdSetRenderTargets
    {
        DECLARE_COMMAND_VALUE;
        RTV* pRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        DSV* pDSV;
    };
    struct CmdSetUAV
    {
        DECLARE_COMMAND_VALUE;
        bool graphics;
        UINT slot;
        UAV* pUAV;
        UINT initialCount;
    };
    struct CmdSetStencilRef
    {
        DECLARE_COMMAND_VALUE;
        UINT ref;
    };
    struct CmdSetBlendFactor
    {
        DECLARE_COMMAND_VALUE;
        float factor[4];
    };
    struct CmdSetViewport
    {
        DECLARE_COMMAND_VALUE;
        UINT slot;
        D3D12_VIEWPORT viewport;
    };
    struct CmdSetNumViewports
    {
        DECLARE_COMMAND_VALUE;
        UINT num;
    };
    struct CmdSetScissorRect
    {
        DECLARE_COMMAND_VALUE;
        UINT slot;
        D3D12_RECT rect;
    };
    struct CmdSetNumScissorRects
    {
        DECLARE_COMMAND_VALUE;
        UINT num;
    };
    struct CmdSetScissorEnable
    {
        DECLARE_COMMAND_VALUE;
        bool enable;
    };
    template <typename View, typename ColorType = FLOAT>
    struct CmdClearView
    {
        View* pView;
        ColorType color[4];
        UINT numRects;
        // Followed by D3D12_RECT[numRects]
        CmdClearView(View* _pView, const ColorType _color[4], UINT _numRects) : pView(_pView), numRects(_numRects) { std::copy(_color, _color + 4, color); }
    };
    struct CmdClearRenderTargetView : CmdClearView<RTV>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearRenderTargetView(RTV* _pView, const FLOAT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdClearDepthStencilView
    {
        DECLARE_COMMAND_VALUE;
        DSV* pView;
        UINT flags;
        FLOAT depth;
        UINT8 stencil;
        UINT numRects;
        // Followed by D3D12_RECT[numRects]
    };
    struct CmdClearUnorderedAccessViewUint : CmdClearView<UAV, UINT>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearUnorderedAccessViewUint(UAV* _pView, const UINT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdClearUnorderedAccessViewFloat : CmdClearView<UAV, FLOAT>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearUnorderedAccessViewFloat(UAV* _pView, const FLOAT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdClearVideoDecoderOutputView : CmdClearView<VDOV>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearVideoDecoderOutputView(VDOV* _pView, const FLOAT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdClearVideoProcessorInputView : CmdClearView<VPIV>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearVideoProcessorInputView(VPIV* _pView, const FLOAT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdClearVideoProcessorOutputView : CmdClearView<VPOV>
    {
        DECLARE_COMMAND_VALUE;
        CmdClearVideoProcessorOutputView(VPOV* _pView, const FLOAT _color[4], UINT _numRects) : CmdClearView(_pView, _color, _numRects) { }
    };
    struct CmdDiscardView
    {
        DECLARE_COMMAND_VALUE;
        ViewBase* pView;
        UINT numRects;
        // Followed by D3D12_RECT[numRects]
    };
    struct CmdDiscardResource
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        UINT numRects;
        // Followed by D3D12_RECT[numRects]
    };
    struct CmdGenMips
    {
        DECLARE_COMMAND_VALUE;
        SRV* pSRV;
        D3D12_FILTER_TYPE filterType;
    };
    struct CmdFinalizeUpdateSubresources
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        ImmediateContext::PreparedUpdateSubresourcesOperation Op;
    };
    struct CmdFinalizeUpdateSubresourcesWithLocalPlacement
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        ImmediateContext::PreparedUpdateSubresourcesOperationWithLocalPlacement Op;
    };
    struct CmdRename
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        Resource* pRenameResource;
    };
    struct CmdRenameViaCopy
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        Resource* pRenameResource;
        UINT dirtyPlaneMask;
    };
    struct CmdQueryBegin
    {
        DECLARE_COMMAND_VALUE;
        Async* pQuery;
    };
    struct CmdQueryEnd
    {
        DECLARE_COMMAND_VALUE;
        Async* pQuery;
    };
    struct CmdSetPredication
    {
        DECLARE_COMMAND_VALUE;
        Query* pPredicate;
        BOOL Value;
    };
    struct CmdResourceCopy
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        Resource* pSrc;
    };
    struct CmdResolveSubresource
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        Resource* pSrc;
        UINT DstSubresource;
        UINT SrcSubresource;
        DXGI_FORMAT Format;
    };
    struct CmdResourceCopyRegion
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        Resource* pSrc;
        UINT DstSubresource;
        UINT SrcSubresource;
        UINT DstX, DstY, DstZ;
        D3D12_BOX SrcBox;
    };
    struct CmdSetResourceMinLOD
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        FLOAT MinLOD;
    };
    struct CmdCopyStructureCount
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDst;
        UAV* pSrc;
        UINT DstOffset;
    };
    struct CmdRotateResourceIdentities
    {
        DECLARE_COMMAND_VALUE;
        UINT NumResources;
        // Followed by Resource*[NumResources]
    };
    struct CmdExtension
    {
        DECLARE_COMMAND_VALUE;
        UINT _CmdValue = CmdValue;
        BatchedExtension* pExt;
        size_t DataSize;
        CmdExtension(BatchedExtension*, const void*, size_t);
        static size_t GetCommandSize(BatchedExtension*, const void*, size_t DataSize);
    };
    struct CmdSetHardwareProtection
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        UINT Value;
    };
    struct CmdSetHardwareProtectionState
    {
        DECLARE_COMMAND_VALUE;
        BOOL State;
    };
    struct CmdClearState
    {
        DECLARE_COMMAND_VALUE;
    };
    struct CmdUpdateTileMappings
    {
        DECLARE_COMMAND_VALUE;
        UINT _CmdValue = CmdValue;
        UINT NumTiledResourceRegions;
        UINT NumRanges;
        ImmediateContext::TILE_MAPPING_FLAG Flags;
        Resource* pTiledResource;
        Resource* pTilePool;
        bool bTiledResourceRegionSizesPresent;
        bool bRangeFlagsPresent;
        bool bTilePoolStartOffsetsPresent;
        bool bRangeTileCountsPresent;
        CmdUpdateTileMappings(Resource*, UINT, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*,
                              Resource*, UINT, const ImmediateContext::TILE_RANGE_FLAG*,
                              const UINT*, const UINT*, ImmediateContext::TILE_MAPPING_FLAG);
        static size_t GetCommandSize(Resource*, UINT, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*,
                                     Resource*, UINT, const ImmediateContext::TILE_RANGE_FLAG*,
                                     const UINT*, const UINT*, ImmediateContext::TILE_MAPPING_FLAG);
        // Followed by D3D12_TILED_RESOURCE_COORDINATE[NumTiledResourceRegions], D3D12_TILE_REGION_SIZE[NumTiledResourceRegions],
        // TILE_RANGE_FLAG[NumRanges], UINT[NumRanges], UINT[NumRanges]
    };
    struct CmdCopyTileMappings
    {
        DECLARE_COMMAND_VALUE;
        Resource* pDstTiledResource;
        Resource* pSrcTiledResource;
        D3D12_TILED_RESOURCE_COORDINATE DstStartCoords;
        D3D12_TILED_RESOURCE_COORDINATE SrcStartCoords;
        D3D12_TILE_REGION_SIZE TileRegion;
        ImmediateContext::TILE_MAPPING_FLAG Flags;
    };
    struct CmdCopyTiles
    {
        DECLARE_COMMAND_VALUE;
        Resource* pResource;
        Resource* pBuffer;
        D3D12_TILED_RESOURCE_COORDINATE StartCoords;
        D3D12_TILE_REGION_SIZE TileRegion;
        UINT64 BufferOffset;
        ImmediateContext::TILE_COPY_FLAG Flags;
    };
    struct CmdTiledResourceBarrier
    {
        DECLARE_COMMAND_VALUE;
        Resource* pBefore;
        Resource* pAfter;
    };
    struct CmdResizeTilePool
    {
        DECLARE_COMMAND_VALUE;
        Resource* pTilePool;
        UINT64 NewSize;
    };
    struct CmdExecuteNestedBatch
    {
        DECLARE_COMMAND_VALUE;
        Batch* pBatch;
        BatchedContext* pThis;
    };
    struct CmdSetMarker
    {
        DECLARE_COMMAND_VALUE;
        UINT NumChars;
        // Followed by wchar_t[NumChars]
    };
    struct CmdBeginEvent
    {
        DECLARE_COMMAND_VALUE;
        UINT NumChars;
        // Followed by wchar_t[NumChars]
    };
    struct CmdEndEvent
    {
        DECLARE_COMMAND_VALUE;
    };

    // The 0-based value of the last command.
    static constexpr UINT c_LastCommand = __COUNTER__ - c_FirstCommandCounterValue - 1;

    struct CreationArgs
    {
        bool CreatesAndDestroysAreMultithreaded : 1;
        bool SubmitBatchesToWorkerThread : 1;
        BatchedContext* pParentContext;
    };
    struct Callbacks
    {
        std::function<void(HRESULT)> ThreadErrorCallback;
        std::function<void()> PostSubmitCallback;
    };

    BatchedContext(ImmediateContext& ImmCtx, CreationArgs flags, Callbacks const& callbacks);
    ~BatchedContext();

    bool TRANSLATION_API ProcessBatch();
    bool TRANSLATION_API SubmitBatch(bool bFlushImmCtxAfterBatch = false);
    void TRANSLATION_API SubmitBatchIfIdle(bool bSkipFrequencyCheck = false);

    std::unique_ptr<Batch> TRANSLATION_API FinishBatch(bool bFlushImmCtxAfterBatch = false);
    void TRANSLATION_API SubmitCommandListBatch(Batch*);
    void TRANSLATION_API RetireBatch(std::unique_ptr<Batch>);

    ImmediateContext &FlushBatchAndGetImmediateContext()
    {
        ProcessBatch();
        return m_ImmCtx;
    }

    ImmediateContext &GetImmediateContextNoFlush()
    {
        return m_ImmCtx;
    }

    template <typename TFunc> void AddPostBatchFunction(TFunc&& f)
    {
        auto Lock = m_RecordingLock.TakeLock();
        m_PostBatchFunctions.emplace_back(std::forward<TFunc>(f));
    }
    template <typename T>
    void TRANSLATION_API DeleteObject(T* pObject)
    {
        AddPostBatchFunction([pObject]() { delete pObject; });
    }
    void TRANSLATION_API ReleaseResource(Resource* pResource)
    {
        auto Lock = m_RecordingLock.TakeLock();
        auto Size = pResource->GetResourceSize();
        m_PostBatchFunctions.emplace_back([pResource]() { pResource->Release(); });
        m_PendingDestructionMemorySize += Size;
        if (m_PendingDestructionMemorySize >= 64 * 1024 * 1024 ||
            pResource->Parent()->IsShared())
        {
            SubmitBatch();
        }
    }

    void TRANSLATION_API PostSubmit();

    void TRANSLATION_API SetPipelineState(PipelineState* pPipeline);

    void TRANSLATION_API DrawInstanced(UINT countPerInstance, UINT instanceCount, UINT vertexStart, UINT instanceStart);
    void TRANSLATION_API DrawIndexedInstanced(UINT countPerInstance, UINT instanceCount, UINT indexStart, INT vertexStart, UINT instanceStart);
    void TRANSLATION_API DrawAuto();
    void TRANSLATION_API DrawIndexedInstancedIndirect(Resource*, UINT offset);
    void TRANSLATION_API DrawInstancedIndirect(Resource*, UINT offset);

    void TRANSLATION_API Dispatch(UINT x, UINT y, UINT z);
    void TRANSLATION_API DispatchIndirect(Resource*, UINT offset);

    bool TRANSLATION_API Flush(UINT commandListMask)
    {
        return FlushBatchAndGetImmediateContext().Flush(commandListMask);
    }

    void TRANSLATION_API IaSetTopology(D3D12_PRIMITIVE_TOPOLOGY);
    void TRANSLATION_API IaSetVertexBuffers(UINT, __in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT, Resource**, const UINT*, const UINT*);
    void TRANSLATION_API IaSetIndexBuffer(Resource*, DXGI_FORMAT, UINT offset);

    template<EShaderStage eShader>
    void TRANSLATION_API SetShaderResources(UINT, __in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) UINT, SRV* const*);
    template<EShaderStage eShader>
    void TRANSLATION_API SetSamplers(UINT, __in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) UINT, Sampler**);
    template<EShaderStage eShader>
    void TRANSLATION_API SetConstantBuffers(UINT, __in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT) UINT Buffers, Resource**, __in_ecount_opt(Buffers) CONST UINT* pFirstConstant, __in_ecount_opt(Buffers) CONST UINT* pNumConstants);

    void TRANSLATION_API SoSetTargets(_In_range_(0, 4) UINT NumTargets, _In_range_(0, 4) UINT, _In_reads_(NumTargets) Resource**, _In_reads_(NumTargets) const UINT*);
    void TRANSLATION_API OMSetRenderTargets(__in_ecount(NumRTVs) RTV** pRTVs, __in_range(0, 8) UINT NumRTVs, __in_opt DSV *, __in_ecount(NumUavs) UAV ** pUavs, CONST UINT*, UINT, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumUavs);
    void TRANSLATION_API CsSetUnorderedAccessViews(UINT, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV**, __in_ecount(NumViews) CONST UINT*);

    void TRANSLATION_API OMSetStencilRef(UINT);
    void TRANSLATION_API OMSetBlendFactor(const FLOAT[4]);

    void TRANSLATION_API SetViewports(UINT, const D3D12_VIEWPORT*);
    void TRANSLATION_API SetScissorRects(UINT, const D3D12_RECT*);
    void TRANSLATION_API SetScissorRectEnable(BOOL);

    void TRANSLATION_API ClearRenderTargetView(RTV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearDepthStencilView(DSV *, UINT, FLOAT, UINT8, UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearUnorderedAccessViewUint(UAV *, CONST UINT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearUnorderedAccessViewFloat(UAV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearVideoDecoderOutputView(VDOV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearVideoProcessorInputView(VPIV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearVideoProcessorOutputView(VPOV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);

    void TRANSLATION_API DiscardView(ViewBase* pView, const D3D12_RECT*, UINT);
    void TRANSLATION_API DiscardResource(Resource* pResource, const D3D12_RECT*, UINT);

    void TRANSLATION_API GenMips(SRV *, D3D12_FILTER_TYPE FilterType);

    void TRANSLATION_API ResourceUpdateSubresourceUP(Resource* pResource, UINT DstSubresource, _In_opt_ const D3D12_BOX* pDstBox, _In_ const VOID* pMem, UINT SrcPitch, UINT SrcDepth);
    void TRANSLATION_API UploadInitialData(Resource* pDst,
                                           D3D12TranslationLayer::CSubresourceSubset const& Subresources,
                                           _In_reads_opt_(_Inexpressible_(Subresources.NumNonExtendedSubresources())) const D3D11_SUBRESOURCE_DATA* pSrcData,
                                           _In_opt_ const D3D12_BOX* pDstBox);

    void TRANSLATION_API QueryBegin(BatchedQuery*);
    void TRANSLATION_API QueryEnd(BatchedQuery*);
    bool TRANSLATION_API QueryGetData(BatchedQuery*, void*, UINT, bool DoNotFlush);
    void TRANSLATION_API SetPredication(Query*, BOOL);

    // Map methods

    // Make sure the batch is completed, then call into the immediate context to wait for the GPU.
    bool TRANSLATION_API MapUnderlyingSynchronize(BatchedResource* pResource, UINT Subresource, MAP_TYPE, bool, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource*);
    // Make sure the batch is completed, then call into the immediate context to figure out how to do the map.
    bool TRANSLATION_API MapDefault(BatchedResource* pResource, UINT Subresource, MAP_TYPE, bool, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource*);
    // Call thread-safe immediate context methods to acquire a mappable buffer, and queue a rename operation.
    bool TRANSLATION_API RenameAndMapBuffer(BatchedResource* pResource, MappedSubresource*);
    // Call thread-safe immediate context methods to acquire a mappable buffer - don't queue anything yet.
    bool TRANSLATION_API MapForRenameViaCopy(BatchedResource* pResource, UINT Subresource, MappedSubresource*);
    // Re-map the last-acquired buffer associated with a resource.
    bool TRANSLATION_API MapRenamedBuffer(BatchedResource* pResource, MappedSubresource*);

    // Unmap methods

    // Just unmap the renamed buffer.
    void TRANSLATION_API UnmapRenamedBuffer(BatchedResource* pResource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    // Map enforced synchronization, just forward to immediate context.
    void TRANSLATION_API UnmapDefault(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    // Map enforced synchronization, just forward to immediate context.
    void TRANSLATION_API UnmapStaging(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    // Unmap the buffer and queue a rename-via-copy operation.
    void TRANSLATION_API UnmapAndRenameViaCopy(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);

    void TRANSLATION_API ResourceCopy(Resource*, Resource*);
    void TRANSLATION_API ResourceResolveSubresource(Resource*, UINT, Resource*, UINT, DXGI_FORMAT);
    void TRANSLATION_API ResourceCopyRegion(Resource*, UINT, UINT, UINT, UINT, Resource*, UINT, const D3D12_BOX*);
    void TRANSLATION_API SetResourceMinLOD(Resource*, FLOAT);
    void TRANSLATION_API CopyStructureCount(Resource*, UINT, UAV*);

    void TRANSLATION_API UpdateTileMappings(Resource* hTiledResource, UINT NumTiledResourceRegions,
                                            _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords,
                                            _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
                                            Resource* hTilePool, UINT NumRanges,
                                            _In_reads_opt_(NumRanges) const ImmediateContext::TILE_RANGE_FLAG* pRangeFlags,
                                            _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets,
                                            _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts,
                                            ImmediateContext::TILE_MAPPING_FLAG Flags);
    void TRANSLATION_API CopyTileMappings(Resource* pDstTiledResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstStartCoords, Resource* pSrcTiledResource, _In_ const  D3D12_TILED_RESOURCE_COORDINATE* pSrcStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, ImmediateContext::TILE_MAPPING_FLAG Flags);
    void TRANSLATION_API CopyTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, Resource* pBuffer, UINT64 BufferOffset, ImmediateContext::TILE_COPY_FLAG Flags);
    void TRANSLATION_API UpdateTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pCoord, _In_ const D3D12_TILE_REGION_SIZE* pRegion, const _In_ VOID* pData, UINT Flags);
    void TRANSLATION_API TiledResourceBarrier(Resource* pBefore, Resource* pAfter);
    void TRANSLATION_API ResizeTilePool(Resource* pResource, UINT64 NewSize);

    void TRANSLATION_API RotateResourceIdentities(Resource* const* ppResources, UINT Resources);

    void TRANSLATION_API SetHardwareProtection(Resource* pResource, UINT value);
    void TRANSLATION_API SetHardwareProtectionState(BOOL state);

    void TRANSLATION_API ClearState();

    void TRANSLATION_API SetMarker(const wchar_t* name);
    void TRANSLATION_API BeginEvent(const wchar_t* name);
    void TRANSLATION_API EndEvent();

    void TRANSLATION_API BatchExtension(BatchedExtension* pExt, const void* pData, size_t DataSize);
    template <typename T> void BatchExtension(BatchedExtension* pExt, T const& Data)
    {
        static_assert(std::is_trivially_destructible<T>::value, "Destructors don't get called on batched commands.");
        static_assert(std::is_trivially_copyable<T>::value, "Extensions must be trivially copyable.");
        BatchExtension(pExt, &Data, sizeof(Data));
    }
    template <typename TExt, typename... Args> void EmplaceBatchExtension(BatchedExtension* pExt, Args&&... args)
    {
        assert(!IsBatchThread());
        auto Lock = m_RecordingLock.TakeLock();
        static_assert(std::is_trivially_destructible<TExt>::value, "Destructors don't get called on batched commands.");
        const size_t ExtensionSize = TExt::GetExtensionSize(std::forward<Args>(args)...);
        const size_t CommandSize = CmdExtension::GetCommandSize(pExt, nullptr, ExtensionSize);

        if (!m_CurrentBatch.reserve_contiguous(CommandSize / sizeof(BatchPrimitive)))
        {
            throw std::bad_alloc();
        }

        void* pPtr = m_CurrentBatch.append_contiguous_manually(CommandSize / sizeof(BatchPrimitive));
        auto pExtensionCmd = new (pPtr) CmdExtension(pExt, nullptr, ExtensionSize);
        new (AlignPtr(pExtensionCmd + 1)) TExt(std::forward<Args>(args)...);
        ++m_CurrentCommandCount;
        SubmitBatchIfIdle();
    }

    using DispatcherFunction = void(*)(ImmediateContext&, const void*&);

    void ProcessBatchImpl(Batch* pBatchToProcess);

private:
    ImmediateContext& m_ImmCtx;
    const DispatcherFunction* const m_DispatchTable;
    const CreationArgs m_CreationArgs;

    template <typename TCmd> void AddToBatch(BatchStorage& CurrentBatch, TCmd const& command);
    template <typename TCmd> void AddToBatch(TCmd const& command)
    {
        auto Lock = m_RecordingLock.TakeLock();
        AddToBatch(m_CurrentBatch, command);

        ++m_CurrentCommandCount;
        SubmitBatchIfIdle();
    }
    template <typename TCmd, typename TEntry> void AddToBatchVariableSize(TCmd const& command, UINT NumEntries, TEntry const* entries);
    template <typename TCmd, typename... Args> void EmplaceInBatch(Args&&... args);
    void ProcessBatchWork(BatchStorage& batch);

    bool WaitForBatchThreadIdle();
    bool IsBatchThreadIdle();
    bool WaitForSingleBatch(DWORD timeout);
    bool IsBatchThread();

    void BatchThread();

    template <typename TFunc>
    bool SyncWithBatch(uint64_t& BatchID, bool DoNotFlush, TFunc&& GetImmObjectFenceValues);

    std::unique_ptr<Batch> GetIdleBatch();

private: // Referenced by recording and batch threads
    SafeHANDLE m_BatchThread;
    SafeHANDLE m_BatchSubmittedSemaphore; // Signaled by recording thread to indicate new work available.
    SafeHANDLE m_BatchConsumedSemaphore; // Signaled by batch thread to indicate it's completed work, waited on by main thread when work submitted.

    OptLock<> m_SubmissionLock{ m_CreationArgs.SubmitBatchesToWorkerThread }; // Synchronizes the deques and free page list.
    std::deque<std::unique_ptr<Batch>> m_QueuedBatches;
    std::deque<std::unique_ptr<Batch>> m_FreeBatches;

    // Note: Must be declared before BatchStorageAllocator
    FreePageContainer m_FreePages{ m_CreationArgs.SubmitBatchesToWorkerThread };

    uint64_t m_CompletedBatchID = 0;

    const Callbacks m_Callbacks;
    std::atomic<bool> m_bFlushPendingCallback;

private: // Referenced by recording thread
    CBoundState<UAV, D3D11_1_UAV_SLOT_COUNT> m_UAVs;
    UINT m_NumScissors = 0;
    D3D12_RECT m_Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT m_NumViewports = 0;
    D3D12_VIEWPORT m_Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};

    void ClearStateImpl();

    const BatchStorageAllocator m_BatchStorageAllocator
    {
        m_CreationArgs.pParentContext ? &m_CreationArgs.pParentContext->m_FreePages :
            (m_CreationArgs.SubmitBatchesToWorkerThread ? &m_FreePages : nullptr)
    };

    static constexpr UINT c_CommandKickoffMinThreshold = 10; // Arbitrary for now
    OptLock<std::recursive_mutex> m_RecordingLock{ m_CreationArgs.CreatesAndDestroysAreMultithreaded };
    UINT m_CurrentCommandCount = 0;
    UINT m_NumOutstandingBatches = 0;
    uint64_t m_RecordingBatchID = 1;

    BatchStorage m_CurrentBatch{ m_BatchStorageAllocator };

private: // Written by non-recording application threads, read by recording thread
    std::vector<std::function<void()>> m_PostBatchFunctions;
    uint64_t m_PendingDestructionMemorySize = 0;
};

struct BatchedDeleter
{
    BatchedContext& Context;
    template <typename TPtr> void operator()(TPtr* pObject) { Context.DeleteObject(pObject); }
};

template <typename TPtr>
using unique_batched_ptr = std::unique_ptr<TPtr, BatchedDeleter>;

class BatchedDeviceChild
{
public:
    BatchedDeviceChild(BatchedContext& Parent) noexcept
        : m_Parent(Parent)
    {
    }

    void ProcessBatch();
    BatchedContext& m_Parent;
};

template <typename TImmediate>
class BatchedDeviceChildImpl : public BatchedDeviceChild
{
public:
    template <typename... Args>
    BatchedDeviceChildImpl(BatchedContext& Parent, Args&&... args)
        : BatchedDeviceChild(Parent)
        , m_pImmediate(new TImmediate(&Parent.GetImmediateContextNoFlush(), std::forward<Args>(args)...),
                      BatchedDeleter{ Parent })
    {
    }

    TImmediate& FlushBatchAndGetImmediate() { ProcessBatch(); return *m_pImmediate; }
    TImmediate& GetImmediateNoFlush() { return *m_pImmediate; }

protected:
    unique_batched_ptr<TImmediate> m_pImmediate;
};


}