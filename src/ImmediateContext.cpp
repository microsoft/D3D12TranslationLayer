// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "PrecompiledShaders.h"

namespace D3D12TranslationLayer
{

//==================================================================================================================================
// 
//==================================================================================================================================

void ImmediateContext::SStageState::ClearState(EShaderStage stage) noexcept
{
    m_CBs.Clear(stage);
    m_SRVs.Clear(stage);
    m_Samplers.Clear();
}

void ImmediateContext::SState::ClearState() noexcept
{
    for (EShaderStage stage = (EShaderStage)0; stage < ShaderStageCount; stage = (EShaderStage)(stage + 1))
    {
        GetStageState(stage).ClearState(stage);
    }

    m_UAVs.Clear(e_Graphics);
    m_CSUAVs.Clear(e_Compute);

    m_RTVs.Clear(e_Graphics);
    m_DSVs.Clear(e_Graphics);
    m_VBs.Clear(e_Graphics);
    m_IB.Clear(e_Graphics);
    m_SO.Clear(e_Graphics);
    m_pPredicate = nullptr;
    m_pPSO = nullptr;
}

ImmediateContext::SStageState& ImmediateContext::SState::GetStageState(EShaderStage stage) noexcept
{
    switch(stage)
    {
        case e_PS: return m_PS;
        case e_VS: return m_VS;
        case e_GS: return m_GS;
        case e_HS: return m_HS;
        case e_DS: return m_DS;
        case e_CS: return m_CS;
        default: ASSUME(false);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::ImmediateContext(UINT nodeIndex, D3D12_FEATURE_DATA_D3D12_OPTIONS& caps, 
    ID3D12Device* pDevice, ID3D12CommandQueue* pQueue, TranslationLayerCallbacks const& callbacks, UINT64 debugFlags, CreationArgs args) noexcept(false)
    : m_nodeIndex(nodeIndex)
    , m_caps(caps)
    , m_FeatureLevel(GetHardwareFeatureLevel(pDevice))
    , m_pDevice12(pDevice)
    , m_SRVAllocator(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, args.CreatesAndDestroysAreMultithreaded, 1 << nodeIndex)
    , m_UAVAllocator(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, args.CreatesAndDestroysAreMultithreaded, 1 << nodeIndex)
    , m_RTVAllocator(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, args.CreatesAndDestroysAreMultithreaded, 1 << nodeIndex)
    , m_DSVAllocator(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, args.CreatesAndDestroysAreMultithreaded, 1 << nodeIndex)
    , m_SamplerAllocator(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, args.CreatesAndDestroysAreMultithreaded, 1 << nodeIndex)
    , m_ResourceCache(*this)
    , m_DirtyStates(e_DirtyOnFirstCommandList)
    , m_StatesToReassert(e_ReassertOnNewCommandList)
    , m_UploadBufferPool(m_BufferPoolTrimThreshold, args.CreatesAndDestroysAreMultithreaded)
    , m_ReadbackBufferPool(m_BufferPoolTrimThreshold, args.CreatesAndDestroysAreMultithreaded)
    , m_DecoderBufferPool(m_BufferPoolTrimThreshold, args.CreatesAndDestroysAreMultithreaded)
    , m_uStencilRef(0)
    , m_PrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    , m_PredicateValue(false)
    , m_IndexBufferFormat(DXGI_FORMAT_UNKNOWN)
    , m_uNumScissors(0)
    , m_uNumViewports(0)
    , m_ScissorRectEnable(false)
    , m_uIndexBufferOffset(0)
    , m_callbacks(callbacks)
    , m_GenerateMipsRootSig(this)
    , m_InternalUAVRootSig(this)
    , m_DeferredDeletionQueueManager(this)

    , m_UploadHeapSuballocator(
        std::forward_as_tuple(cBuddyMaxBlockSize, cBuddyAllocatorThreshold, (bool)args.CreatesAndDestroysAreMultithreaded, this, AllocatorHeapType::Upload),
        std::forward_as_tuple(this, AllocatorHeapType::Upload),
        ResourceNeedsOwnAllocation)

    , m_ReadbackHeapSuballocator(
        std::forward_as_tuple(cBuddyMaxBlockSize, cBuddyAllocatorThreshold, (bool)args.CreatesAndDestroysAreMultithreaded, this, AllocatorHeapType::Readback),
        std::forward_as_tuple(this, AllocatorHeapType::Readback),
        ResourceNeedsOwnAllocation)

    , m_DecoderHeapSuballocator(
        std::forward_as_tuple(cBuddyMaxBlockSize, cBuddyAllocatorThreshold, (bool)args.CreatesAndDestroysAreMultithreaded, this, AllocatorHeapType::Decoder),
        std::forward_as_tuple(this, AllocatorHeapType::Decoder),
        ResourceNeedsOwnAllocation)

    , m_CreationArgs(args)
    , m_ResourceStateManager(*this)
#if DBG
    , m_DebugFlags(debugFlags)
#endif
    , m_bUseRingBufferDescriptorHeaps(args.IsXbox)
    , m_BltResolveManager(*this)
    , m_residencyManager(*this)
{
    UNREFERENCED_PARAMETER(debugFlags);
    memset(m_BlendFactor, 0, sizeof(m_BlendFactor));
    memset(m_auVertexOffsets, 0, sizeof(m_auVertexOffsets));
    memset(m_auVertexStrides, 0, sizeof(m_auVertexStrides));
    memset(m_aScissors, 0, sizeof(m_aScissors));
    memset(m_aViewports, 0, sizeof(m_aViewports));

    HRESULT hr = S_OK;
    if (!m_CreationArgs.UseResidencyManagement)
    {
        // Residency management is no longer optional
        ThrowFailure(E_INVALIDARG);
    }

    if (m_CreationArgs.RenamingIsMultithreaded)
    {
        m_RenamesInFlight.InitLock();
    }

    if (m_CreationArgs.UseThreadpoolForPSOCreates)
    {
        m_spPSOCompilationThreadPool.reset(new CThreadPool);
    }

    if (m_CreationArgs.CreatesAndDestroysAreMultithreaded)
    {
        m_DeferredDeletionQueueManager.InitLock();
    }

    m_MaxFrameLatencyHelper.Init(this);

    D3D12TranslationLayer::InitializeListHead(&m_ActiveQueryList);

    D3D12_COMMAND_QUEUE_DESC SyncOnlyQueueDesc = { D3D12_COMMAND_LIST_TYPE_NONE };
    (void)m_pDevice12->CreateCommandQueue(&SyncOnlyQueueDesc, IID_PPV_ARGS(&m_pSyncOnlyQueue));

    LUID adapterLUID = pDevice->GetAdapterLuid();
    {
        CComPtr<IDXCoreAdapterFactory> pFactory;
#if DYNAMIC_LOAD_DXCORE
        m_DXCore.load("dxcore");
        auto pfnDXCoreCreateAdapterFactory = m_DXCore.proc_address<HRESULT(APIENTRY*)(REFIID, void**)>("DXCoreCreateAdapterFactory");
        if (m_DXCore && pfnDXCoreCreateAdapterFactory && SUCCEEDED(pfnDXCoreCreateAdapterFactory(IID_PPV_ARGS(&pFactory))))
#else
        if (SUCCEEDED(DXCoreCreateAdapterFactory(IID_PPV_ARGS(&pFactory))))
#endif
        {
            (void)pFactory->GetAdapterByLuid(adapterLUID, IID_PPV_ARGS(&m_pDXCoreAdapter));
        }
    }
    if (!m_pDXCoreAdapter)
    {
        CComPtr<IDXGIFactory4> pFactory;
        ThrowFailure(CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory)));
        ThrowFailure(pFactory->EnumAdapterByLuid(adapterLUID, IID_PPV_ARGS(&m_pDXGIAdapter)));
    }

    m_residencyManager.Initialize(nodeIndex, m_pDXCoreAdapter.get(), m_pDXGIAdapter.get());

    m_UAVDeclScratch.reserve(D3D11_1_UAV_SLOT_COUNT); // throw( bad_alloc )
    m_vUAVBarriers.reserve(D3D11_1_UAV_SLOT_COUNT); // throw( bad_alloc )

    m_ViewHeap.m_MaxHeapSize = min((DWORD) D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, m_CreationArgs.MaxSRVHeapSize);
    if (m_ViewHeap.m_MaxHeapSize == 0)
        m_ViewHeap.m_MaxHeapSize = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
    const UINT32 viewHeapStartingCount = m_bUseRingBufferDescriptorHeaps ? 4096 : m_ViewHeap.m_MaxHeapSize;
    m_ViewHeap.m_DescriptorRingBuffer = CFencedRingBuffer(viewHeapStartingCount);
    m_ViewHeap.m_Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    m_ViewHeap.m_Desc.NumDescriptors = viewHeapStartingCount;
    m_ViewHeap.m_Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_ViewHeap.m_Desc.NodeMask = GetNodeMask();

    m_SamplerHeap.m_MaxHeapSize = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
    const UINT32 samplerHeapStartingCount = m_bUseRingBufferDescriptorHeaps ? 512 : m_SamplerHeap.m_MaxHeapSize;
    m_SamplerHeap.m_DescriptorRingBuffer = CFencedRingBuffer(samplerHeapStartingCount);
    m_SamplerHeap.m_Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    m_SamplerHeap.m_Desc.NumDescriptors = samplerHeapStartingCount;
    m_SamplerHeap.m_Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_SamplerHeap.m_Desc.NodeMask = GetNodeMask();

    // Create initial objects
    hr = m_pDevice12->CreateDescriptorHeap(&m_ViewHeap.m_Desc, IID_PPV_ARGS(&m_ViewHeap.m_pDescriptorHeap));
    ThrowFailure(hr); //throw( _com_error )
    m_ViewHeap.m_DescriptorSize = m_pDevice12->GetDescriptorHandleIncrementSize(m_ViewHeap.m_Desc.Type);
    m_ViewHeap.m_DescriptorHeapBase = m_ViewHeap.m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
    m_ViewHeap.m_DescriptorHeapBaseCPU = m_ViewHeap.m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
    m_ViewHeap.m_BitsToSetOnNewHeap = e_ViewsDirty;

    if (!ComputeOnly())
    {
        hr = m_pDevice12->CreateDescriptorHeap(&m_SamplerHeap.m_Desc, IID_PPV_ARGS(&m_SamplerHeap.m_pDescriptorHeap));
        ThrowFailure(hr); //throw( _com_error )
        m_SamplerHeap.m_DescriptorSize = m_pDevice12->GetDescriptorHandleIncrementSize(m_SamplerHeap.m_Desc.Type);
        m_SamplerHeap.m_DescriptorHeapBase = m_SamplerHeap.m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
        m_SamplerHeap.m_DescriptorHeapBaseCPU = m_SamplerHeap.m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
        m_SamplerHeap.m_BitsToSetOnNewHeap = e_SamplersDirty;
    }

    for (UINT i = 0; i <= (UINT)RESOURCE_DIMENSION::TEXTURECUBEARRAY; ++i)
    {
        auto ResourceDimension = ComputeOnly() ? RESOURCE_DIMENSION::BUFFER : (RESOURCE_DIMENSION)i;
        D3D12_SHADER_RESOURCE_VIEW_DESC NullSRVDesc = {};
        D3D12_UNORDERED_ACCESS_VIEW_DESC NullUAVDesc = {};
        NullSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        NullSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        NullUAVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        switch (ResourceDimension)
        {
            case RESOURCE_DIMENSION::BUFFER:
            case RESOURCE_DIMENSION::UNKNOWN:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                NullSRVDesc.Buffer.FirstElement = 0;
                NullSRVDesc.Buffer.NumElements = 0;
                NullSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
                NullSRVDesc.Buffer.StructureByteStride = 0;
                NullUAVDesc.Buffer.FirstElement = 0;
                NullUAVDesc.Buffer.NumElements = 0;
                NullUAVDesc.Buffer.StructureByteStride = 0;
                NullUAVDesc.Buffer.CounterOffsetInBytes = 0;
                NullUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
                if (ComputeOnly())
                {
                    // Compute only will use a raw view instead of typed
                    NullSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    NullUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    NullSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    NullUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                }
                break;
            case RESOURCE_DIMENSION::TEXTURE1D:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                NullSRVDesc.Texture1D.MipLevels = 1;
                NullSRVDesc.Texture1D.MostDetailedMip = 0;
                NullSRVDesc.Texture1D.ResourceMinLODClamp = 0.0f;
                NullUAVDesc.Texture1D.MipSlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURE1DARRAY:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                NullSRVDesc.Texture1DArray.MipLevels = 1;
                NullSRVDesc.Texture1DArray.ArraySize = 1;
                NullSRVDesc.Texture1DArray.MostDetailedMip = 0;
                NullSRVDesc.Texture1DArray.FirstArraySlice = 0;
                NullSRVDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
                NullUAVDesc.Texture1DArray.ArraySize = 1;
                NullUAVDesc.Texture1DArray.MipSlice = 0;
                NullUAVDesc.Texture1DArray.FirstArraySlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURE2D:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                NullSRVDesc.Texture2D.MipLevels = 1;
                NullSRVDesc.Texture2D.MostDetailedMip = 0;
                NullSRVDesc.Texture2D.PlaneSlice = 0;
                NullSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
                NullUAVDesc.Texture2D.MipSlice = 0;                
                NullUAVDesc.Texture2D.PlaneSlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURE2DARRAY:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                NullSRVDesc.Texture2DArray.MipLevels = 1;
                NullSRVDesc.Texture2DArray.ArraySize = 1;
                NullSRVDesc.Texture2DArray.MostDetailedMip = 0;
                NullSRVDesc.Texture2DArray.FirstArraySlice = 0;
                NullSRVDesc.Texture2DArray.PlaneSlice = 0;
                NullSRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
                NullUAVDesc.Texture2DArray.ArraySize = 1;
                NullUAVDesc.Texture2DArray.MipSlice = 0;
                NullUAVDesc.Texture2DArray.FirstArraySlice = 0;
                NullUAVDesc.Texture2DArray.PlaneSlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURE2DMS:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;
                break;
            case RESOURCE_DIMENSION::TEXTURE2DMSARRAY:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;
                NullSRVDesc.Texture2DMSArray.ArraySize = 1;
                NullSRVDesc.Texture2DMSArray.FirstArraySlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURE3D:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                NullSRVDesc.Texture3D.MipLevels = 1;
                NullSRVDesc.Texture3D.MostDetailedMip = 0;
                NullSRVDesc.Texture3D.ResourceMinLODClamp = 0.0f;
                NullUAVDesc.Texture3D.WSize = 1;
                NullUAVDesc.Texture3D.MipSlice = 0;
                NullUAVDesc.Texture3D.FirstWSlice = 0;
                break;
            case RESOURCE_DIMENSION::TEXTURECUBE:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;
                NullSRVDesc.TextureCube.MipLevels = 1;
                NullSRVDesc.TextureCube.MostDetailedMip = 0;
                NullSRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;
                break;
            case RESOURCE_DIMENSION::TEXTURECUBEARRAY:
                NullSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                NullUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;
                NullSRVDesc.TextureCubeArray.MipLevels = 1;
                NullSRVDesc.TextureCubeArray.NumCubes = 1;
                NullSRVDesc.TextureCubeArray.MostDetailedMip = 0;
                NullSRVDesc.TextureCubeArray.First2DArrayFace = 0;
                NullSRVDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
                break;
        }

        if (NullSRVDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN)
        {
            m_NullSRVs[i] = m_SRVAllocator.AllocateHeapSlot(); // throw( _com_error )
            m_pDevice12->CreateShaderResourceView(nullptr, &NullSRVDesc, m_NullSRVs[i]);
        }

        if (NullUAVDesc.ViewDimension != D3D12_UAV_DIMENSION_UNKNOWN)
        {
            m_NullUAVs[i] = m_UAVAllocator.AllocateHeapSlot(); // throw( _com_error )
            m_pDevice12->CreateUnorderedAccessView(nullptr, nullptr, &NullUAVDesc, m_NullUAVs[i]);
        }
    }
    if (!ComputeOnly())
    {
        m_NullRTV = m_RTVAllocator.AllocateHeapSlot(); // throw( _com_error )
        D3D12_RENDER_TARGET_VIEW_DESC NullRTVDesc;
        NullRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        NullRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        NullRTVDesc.Texture2D.MipSlice = 0;
        NullRTVDesc.Texture2D.PlaneSlice = 0;
        m_pDevice12->CreateRenderTargetView(nullptr, &NullRTVDesc, m_NullRTV);
    }

    if (!ComputeOnly())
    {
        m_NullSampler = m_SamplerAllocator.AllocateHeapSlot(); // throw( _com_error )
        // Arbitrary parameters used, this sampler should never actually be used
        D3D12_SAMPLER_DESC NullSamplerDesc;
        NullSamplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        NullSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        NullSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        NullSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        NullSamplerDesc.MipLODBias = 0.0f;
        NullSamplerDesc.MaxAnisotropy = 0;
        NullSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        NullSamplerDesc.MinLOD = 0.0f;
        NullSamplerDesc.MaxLOD = 0.0f;
        memset(NullSamplerDesc.BorderColor, 0, sizeof(NullSamplerDesc.BorderColor));
        m_pDevice12->CreateSampler(&NullSamplerDesc, m_NullSampler);
    }

    (void)m_pDevice12->QueryInterface(&m_pDevice12_1);
    (void)m_pDevice12->QueryInterface(&m_pDevice12_2);
    m_pDevice12->QueryInterface(&m_pCompatDevice);

    m_CommandLists[(UINT)COMMAND_LIST_TYPE::GRAPHICS].reset(new CommandListManager(this, pQueue, COMMAND_LIST_TYPE::GRAPHICS)); // throw( bad_alloc )
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::GRAPHICS]->InitCommandList();
}

bool ImmediateContext::Shutdown() noexcept
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if (m_CommandLists[i])
        {
            // The device is being destroyed so no point executing any authored work
            m_CommandLists[i]->DiscardCommandList();

            // Make sure any GPU work still in the pipe is finished
            try {
                if (!m_CommandLists[i]->WaitForCompletion()) // throws
                {
                    return false;
                }
            }
            catch (_com_error&)
            {
                return false;
            }
            catch (std::bad_alloc&)
            {
                return false;
            }
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::~ImmediateContext() noexcept
{
    Shutdown();

    //Ensure all remaining allocations are cleaned up
    TrimDeletedObjects(true);

    // All queries should be gone by this point
    assert(D3D12TranslationLayer::IsListEmpty(&m_ActiveQueryList));
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::InitializeVideo(ID3D12VideoDevice **ppVideoDevice)
{
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_DECODE].reset(new CommandListManager(this, nullptr, COMMAND_LIST_TYPE::VIDEO_DECODE)); // throw( bad_alloc )
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_DECODE]->InitCommandList();

    m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_PROCESS].reset(new CommandListManager(this, nullptr, COMMAND_LIST_TYPE::VIDEO_PROCESS)); // throw( bad_alloc )
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_PROCESS]->InitCommandList();

    ThrowFailure(m_pDevice12_1->QueryInterface(ppVideoDevice));
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::AddResourceToDeferredDeletionQueue(ID3D12Object* pUnderlying, std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired, std::vector<DeferredWait> deferredWaits)
{
    // Note: Due to the below routines being called after deferred deletion queue destruction,
    // all callers of the generic AddObjectToQueue should ensure that the object really needs to be in the queue.
    if (!RetiredD3D12Object::ReadyToDestroy(this, completionRequired, lastCommandListIDs, deferredWaits))
    {
        m_DeferredDeletionQueueManager.GetLocked()->AddObjectToQueue(pUnderlying, std::move(pResidencyHandle), lastCommandListIDs, completionRequired, std::move(deferredWaits));
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::AddObjectToDeferredDeletionQueue(ID3D12Object* pUnderlying, COMMAND_LIST_TYPE commandListType, UINT64 lastCommandListID, bool completionRequired)
{
    // Note: May be called after the deferred deletion queue has been destroyed, but in all such cases,
    // the ReadyToDestroy function will return true.
    if (!RetiredD3D12Object::ReadyToDestroy(this, completionRequired, lastCommandListID, commandListType))
    {
        std::unique_ptr<ResidencyManagedObjectWrapper> nullUniquePtr;
        m_DeferredDeletionQueueManager.GetLocked()->AddObjectToQueue(pUnderlying, std::move(nullUniquePtr), commandListType, lastCommandListID, completionRequired);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::AddObjectToDeferredDeletionQueue(ID3D12Object* pUnderlying, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired)
{
    // Note: May be called after the deferred deletion queue has been destroyed, but in all such cases,
    // the ReadyToDestroy function will return true.
    if (!RetiredD3D12Object::ReadyToDestroy(this, completionRequired, lastCommandListIDs))
    {
        std::unique_ptr<ResidencyManagedObjectWrapper> nullUniquePtr;
        m_DeferredDeletionQueueManager.GetLocked()->AddObjectToQueue(pUnderlying, std::move(nullUniquePtr), lastCommandListIDs, completionRequired);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
bool DeferredDeletionQueueManager::TrimDeletedObjects(bool deviceBeingDestroyed)
{
    bool AnyObjectsDestroyed = false;
    while (m_DeferredObjectDeletionQueue.empty() == false &&
        (m_DeferredObjectDeletionQueue.front().ReadyToDestroy(m_pParent) || deviceBeingDestroyed))
    {
        AnyObjectsDestroyed = true;
        m_DeferredObjectDeletionQueue.pop();
    }

    while (SuballocationsReadyToBeDestroyed(deviceBeingDestroyed))
    {
        AnyObjectsDestroyed = true;
        m_DeferredSuballocationDeletionQueue.front().Destroy();
        m_DeferredSuballocationDeletionQueue.pop();
    }

    return AnyObjectsDestroyed;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool DeferredDeletionQueueManager::GetFenceValuesForObjectDeletion(UINT64(&FenceValues)[(UINT)COMMAND_LIST_TYPE::MAX_VALID])
{
    std::fill(FenceValues, std::end(FenceValues), 0ull);
    if (!m_DeferredObjectDeletionQueue.empty())
    {
        auto& obj = m_DeferredObjectDeletionQueue.front();
        std::copy(obj.m_lastCommandListIDs, std::end(obj.m_lastCommandListIDs), FenceValues);
        return true;
    }
    return false;
}

bool DeferredDeletionQueueManager::GetFenceValuesForSuballocationDeletion(UINT64(&FenceValues)[(UINT)COMMAND_LIST_TYPE::MAX_VALID])
{
    std::fill(FenceValues, std::end(FenceValues), 0ull);
    if (!m_DeferredSuballocationDeletionQueue.empty())
    {
        auto& suballocation = m_DeferredSuballocationDeletionQueue.front();
        std::copy(suballocation.m_lastCommandListIDs, std::end(suballocation.m_lastCommandListIDs), FenceValues);
        return true;
    }
    return false;
}

bool DeferredDeletionQueueManager::SuballocationsReadyToBeDestroyed(bool deviceBeingDestroyed)
{
    return m_DeferredSuballocationDeletionQueue.empty() == false &&
        (m_DeferredSuballocationDeletionQueue.front().ReadyToDestroy(m_pParent) || deviceBeingDestroyed);
}

bool ImmediateContext::TrimDeletedObjects(bool deviceBeingDestroyed)
{
    return m_DeferredDeletionQueueManager.GetLocked()->TrimDeletedObjects(deviceBeingDestroyed);
}

bool ImmediateContext::TrimResourcePools()
{
    m_UploadBufferPool.Trim(GetCompletedFenceValue(CommandListType(AllocatorHeapType::Upload)));
    m_ReadbackBufferPool.Trim(GetCompletedFenceValue(CommandListType(AllocatorHeapType::Readback)));
    m_DecoderBufferPool.Trim(GetCompletedFenceValue(CommandListType(AllocatorHeapType::Decoder)));

    return true;
}

void TRANSLATION_API ImmediateContext::PostSubmitNotification()
{
    if (m_callbacks.m_pfnPostSubmit)
    {
        m_callbacks.m_pfnPostSubmit();
    }
    TrimDeletedObjects();
    TrimResourcePools();

    const UINT64 completedFence = GetCompletedFenceValue(COMMAND_LIST_TYPE::GRAPHICS);

    if (m_bUseRingBufferDescriptorHeaps)
    {
        m_ViewHeap.m_DescriptorRingBuffer.Deallocate(completedFence);
        m_SamplerHeap.m_DescriptorRingBuffer.Deallocate(completedFence);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::RollOverHeap(OnlineDescriptorHeap& Heap) noexcept(false)
{
    auto pfnCreateNew = [this](D3D12_DESCRIPTOR_HEAP_DESC const& Desc) -> unique_comptr<ID3D12DescriptorHeap>
    {
        unique_comptr<ID3D12DescriptorHeap> spHeap;
        ThrowFailure(m_pDevice12->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&spHeap)));
        return std::move(spHeap);
    };

    // If we are in the growth phase don't bother using pools
    if (Heap.m_Desc.NumDescriptors < Heap.m_MaxHeapSize)
    {
        // Defer delete the current heap
        AddObjectToDeferredDeletionQueue(Heap.m_pDescriptorHeap.get(), COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS), true);

        // Grow
        Heap.m_Desc.NumDescriptors *= 2;
        Heap.m_Desc.NumDescriptors = min(Heap.m_Desc.NumDescriptors, Heap.m_MaxHeapSize);

        Heap.m_pDescriptorHeap = TryAllocateResourceWithFallback([&]() { return pfnCreateNew(Heap.m_Desc); },
            ResourceAllocationContext::ImmediateContextThreadLongLived);
    }
    else
    {
        // If we reach this point they are really heavy heap users so we can fall back the roll over strategy
        Heap.m_HeapPool.ReturnToPool(std::move(Heap.m_pDescriptorHeap), GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

        UINT64 CurrentFenceValue = GetCompletedFenceValue(COMMAND_LIST_TYPE::GRAPHICS);
        Heap.m_pDescriptorHeap = TryAllocateResourceWithFallback([&]() {
            return Heap.m_HeapPool.RetrieveFromPool(CurrentFenceValue, pfnCreateNew, Heap.m_Desc); // throw( _com_error )
            }, ResourceAllocationContext::ImmediateContextThreadLongLived);
    }

    Heap.m_DescriptorRingBuffer = CFencedRingBuffer(Heap.m_Desc.NumDescriptors);
    Heap.m_DescriptorHeapBase = Heap.m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
    Heap.m_DescriptorHeapBaseCPU = Heap.m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;

    ID3D12DescriptorHeap* pHeaps[2] = {m_ViewHeap.m_pDescriptorHeap.get(), m_SamplerHeap.m_pDescriptorHeap.get()};
    GetGraphicsCommandList()->SetDescriptorHeaps(ComputeOnly() ? 1 : 2, pHeaps);

    m_DirtyStates |= Heap.m_BitsToSetOnNewHeap;
}

//----------------------------------------------------------------------------------------------------------------------------------
UINT ImmediateContext::ReserveSlots(OnlineDescriptorHeap& Heap, UINT NumSlots) noexcept(false)
{
    assert(NumSlots <= Heap.m_Desc.NumDescriptors);

    UINT offset = 0;
    HRESULT hr = S_OK;

    do
    {
        hr = Heap.m_DescriptorRingBuffer.Allocate(NumSlots, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS), offset);

        if (FAILED(hr))
        {
            RollOverHeap(Heap);
        }

    } while (FAILED(hr));

    assert(offset < Heap.m_Desc.NumDescriptors);
    assert(offset + NumSlots <= Heap.m_Desc.NumDescriptors);

    return offset;
}

//----------------------------------------------------------------------------------------------------------------------------------
UINT ImmediateContext::ReserveSlotsForBindings(OnlineDescriptorHeap& Heap, UINT (ImmediateContext::*pfnCalcRequiredSlots)()) noexcept(false)
{
    UINT NumSlots = (this->*pfnCalcRequiredSlots)();
    
    assert(NumSlots <= Heap.m_Desc.NumDescriptors);

    UINT offset = 0;
    HRESULT hr = S_OK;

    do
    {
        hr = Heap.m_DescriptorRingBuffer.Allocate(NumSlots, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS), offset);

        if (FAILED(hr))
        {
            RollOverHeap(Heap);
            NumSlots = (this->*pfnCalcRequiredSlots)();
        }

    } while (FAILED(hr));

    assert(offset < Heap.m_Desc.NumDescriptors);
    assert(offset + NumSlots <= Heap.m_Desc.NumDescriptors);

    return offset;
}

//----------------------------------------------------------------------------------------------------------------------------------
RootSignature* ImmediateContext::CreateOrRetrieveRootSignature(RootSignatureDesc const& desc) noexcept(false)
{
    auto& result = m_RootSignatures[desc];
    if (!result)
    {
        result.reset(new RootSignature(this, desc));
    }
    return result.get();
}

//----------------------------------------------------------------------------------------------------------------------------------
static const D3D12_RECT g_cMaxScissorRect = { D3D12_VIEWPORT_BOUNDS_MIN, D3D12_VIEWPORT_BOUNDS_MIN, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX };
static const D3D12_RECT g_cMaxScissors[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] =
{
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
    g_cMaxScissorRect,
};

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::SetScissorRectsHelper() noexcept
{
    if (ComputeOnly())
    {
        return;
    }
    if (!m_ScissorRectEnable)
    {
        // Set 12 scissor rects to max scissor rects to effectively disable scissor rect culling
        GetGraphicsCommandList()->RSSetScissorRects(D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, g_cMaxScissors);
    }
    else
    {
        // Set 12 scissor rects to 11 scissor rects
        GetGraphicsCommandList()->RSSetScissorRects(m_uNumScissors, reinterpret_cast<const D3D12_RECT*>(m_aScissors));
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::RefreshNonHeapBindings(UINT64 DirtyBits) noexcept
{
    if ((DirtyBits & e_NonHeapBindingsDirty) == 0)
    {
        return;
    }

    if (DirtyBits & e_IndexBufferDirty)
    {
        auto pIB = *m_CurrentState.m_IB.GetBound();
        DXGI_FORMAT fmt = m_IndexBufferFormat == DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R16_UINT : m_IndexBufferFormat;
        m_CurrentState.m_IB.ResetDirty();

        D3D12_INDEX_BUFFER_VIEW IBViewDesc = {};
        IBViewDesc.Format = fmt;
        GetBufferViewDesc(pIB, IBViewDesc, m_uIndexBufferOffset);

        GetGraphicsCommandList()->IASetIndexBuffer(&IBViewDesc);
    }
    if (DirtyBits & e_VertexBuffersDirty)
    {
        const UINT MaxVBs = m_CurrentState.m_VBs.NumBindings;
        D3D12_VERTEX_BUFFER_VIEW VBViewDescs[MaxVBs];
        UINT numVBs = m_CurrentState.m_VBs.GetNumBound();
        UINT numNulls = max(m_CurrentState.m_LastVBCount, numVBs) - numVBs;
        m_CurrentState.m_LastVBCount = numVBs;
        ASSUME(numVBs + numNulls <= MaxVBs);
        m_CurrentState.m_VBs.ResetDirty();

        for (UINT i = 0; i < numVBs; ++i)
        {
            auto pBuffer = m_CurrentState.m_VBs.GetBound()[i];
            UINT APIOffset = m_auVertexOffsets[i];

            GetBufferViewDesc(pBuffer, VBViewDescs[i], APIOffset);
            VBViewDescs[i].StrideInBytes = m_auVertexStrides[i];
        }
        ZeroMemory(&VBViewDescs[numVBs], sizeof(VBViewDescs[0]) * (numNulls));
        GetGraphicsCommandList()->IASetVertexBuffers(0, numVBs + numNulls, VBViewDescs);
    }
    if (DirtyBits & e_StreamOutputDirty)
    {
        const UINT MaxSO = m_CurrentState.m_SO.NumBindings;
        D3D12_STREAM_OUTPUT_BUFFER_VIEW SOViewDescs[ MaxSO ];
        UINT numSOBuffers = m_CurrentState.m_SO.GetNumBound();
        m_CurrentState.m_SO.ResetDirty();

        for (UINT i = 0; i < numSOBuffers; ++i)
        {
            auto pBuffer = m_CurrentState.m_SO.GetBound()[i];
            assert(GetDynamicBufferOffset(pBuffer) == 0); // 11on12 doesn't support renaming stream-output buffers

            GetBufferViewDesc(pBuffer, SOViewDescs[i], 0);

            static_assert(0 == offsetof(SStreamOutputSuffix, BufferFilledSize), "Assumed offset to struct == offset to field");
            SOViewDescs[i].BufferFilledSizeLocation = pBuffer ? (SOViewDescs[i].BufferLocation + pBuffer->GetOffsetToStreamOutputSuffix()) : 0;
        }
        ZeroMemory(&SOViewDescs[numSOBuffers], sizeof(SOViewDescs[0]) * (MaxSO - numSOBuffers));
        GetGraphicsCommandList()->SOSetTargets(0, MaxSO, SOViewDescs);
    }
    if (DirtyBits & e_RenderTargetsDirty)
    {
        const UINT MaxRTVs = m_CurrentState.m_RTVs.NumBindings;
        UINT numRTVs = m_CurrentState.m_RTVs.GetNumBound();
        D3D12_CPU_DESCRIPTOR_HANDLE RTVDescriptors[ MaxRTVs ];
        D3D12_CPU_DESCRIPTOR_HANDLE *pDSVDescriptor = nullptr;
        m_CurrentState.m_RTVs.ResetDirty();

        for (UINT i = 0; i < numRTVs; ++i)
        {
            auto pRTV = m_CurrentState.m_RTVs.GetBound()[i];
            RTVDescriptors[i] = m_NullRTV;
            if (pRTV)
            {
                RTVDescriptors[i] = pRTV->GetRefreshedDescriptorHandle();
            }
        }
        m_CurrentState.m_DSVs.ResetDirty();
        auto pDSV = m_CurrentState.m_DSVs.GetBound()[0];
        if (pDSV)
        {
            pDSVDescriptor = &pDSV->GetRefreshedDescriptorHandle();
        }

        GetGraphicsCommandList()->OMSetRenderTargets(numRTVs, RTVDescriptors, false, pDSVDescriptor);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PreRender(COMMAND_LIST_TYPE type) noexcept
{
    if (type == COMMAND_LIST_TYPE::GRAPHICS)
    {
        // D3D11 predicates do not apply to video
        if (m_StatesToReassert & e_PredicateDirty)
        {
            if (m_CurrentState.m_pPredicate)
            {
                m_CurrentState.m_pPredicate->UsedInCommandList(type, GetCommandListID(type));

                SetPredicationInternal(m_CurrentState.m_pPredicate, m_PredicateValue);
            }
            else
            {
                SetPredicationInternal(nullptr, false);
            }
            AdditionalCommandsAdded(type);
        }
        m_StatesToReassert &= ~(e_PredicateDirty);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PostRender(COMMAND_LIST_TYPE type, UINT64 ReassertBitsToAdd)
{
    m_StatesToReassert |= ReassertBitsToAdd;
    AdditionalCommandsAdded(type);
    GetCommandListManager(type)->SubmitCommandListIfNeeded();

#if DBG
    if (m_DebugFlags & Debug_FlushOnRender  && HasCommands(type))
    {
        SubmitCommandList(type); // throws
    }
#else
    UNREFERENCED_PARAMETER(type);
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PostDraw()
{
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::GRAPHICS]->DrawCommandAdded();
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
#if DBG
    if (m_DebugFlags & Debug_FlushOnDraw && HasCommands(COMMAND_LIST_TYPE::GRAPHICS))
    {
       SubmitCommandList(COMMAND_LIST_TYPE::GRAPHICS);  // throws
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PostDispatch()
{
    m_CommandLists[(UINT)COMMAND_LIST_TYPE::GRAPHICS]->DispatchCommandAdded();
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
#if DBG
    if (m_DebugFlags & Debug_FlushOnDispatch && HasCommands(COMMAND_LIST_TYPE::GRAPHICS))
    {
        SubmitCommandList(COMMAND_LIST_TYPE::GRAPHICS);  // throws
    }
#endif
}

D3D12_BOX ImmediateContext::GetSubresourceBoxFromBox(Resource *pSrc, UINT RequestedSubresource, UINT BaseSubresource, D3D12_BOX const& SrcBox)
{
    assert(BaseSubresource <= RequestedSubresource);
    D3D12_BOX box = SrcBox;

    // This method should not be used with empty rects, as it ensures its output rect is not empty.
    assert(box.left < box.right && box.top < box.bottom && box.front < box.back);

    // RequestedSubresource is a D3D12 subresource, before calling into 11 it needs to be converted back
    UINT ApiRequestedSubresource = RequestedSubresource;
    UINT ApiBaseSubresource = BaseSubresource;
    if (pSrc->SubresourceMultiplier() > 1)
    {
        assert(pSrc->AppDesc()->NonOpaquePlaneCount() == 1);
        ApiRequestedSubresource = ConvertSubresourceIndexRemovePlane(RequestedSubresource, pSrc->AppDesc()->SubresourcesPerPlane());
        ApiBaseSubresource = ConvertSubresourceIndexRemovePlane(BaseSubresource, pSrc->AppDesc()->SubresourcesPerPlane());
    }

    {
        auto& footprint = pSrc->GetSubresourcePlacement(RequestedSubresource).Footprint;
        // Planar textures do not support mipmaps, and in this case coordinates should
        // not be divided by width / height alignment.
        if (pSrc->AppDesc()->NonOpaquePlaneCount() > 1)
        {
            UINT PlaneIndex = GetPlaneIdxFromSubresourceIdx(ApiRequestedSubresource, pSrc->AppDesc()->SubresourcesPerPlane());
            UINT BasePlaneIndex = GetPlaneIdxFromSubresourceIdx(ApiBaseSubresource, pSrc->AppDesc()->SubresourcesPerPlane());

            if (PlaneIndex > 0 && BasePlaneIndex == 0)
            {
                // Adjust for subsampling.
                UINT subsampleX, subsampleY;
                CD3D11FormatHelper::GetYCbCrChromaSubsampling(pSrc->AppDesc()->Format(), subsampleX, subsampleY);
                // Round up on the right bounds to prevent empty rects.
                box.right =  min(footprint.Width,  (box.right  + (subsampleX - 1)) / subsampleX);
                box.left =   min(box.right,         box.left                       / subsampleX);
                box.bottom = min(footprint.Height, (box.bottom + (subsampleY - 1)) / subsampleY);
                box.top =    min(box.bottom,        box.top                        / subsampleY);
            }
            else
            {
                // Make sure the box is at least contained within the subresource.
                box.right =  min(footprint.Width,  box.right);
                box.left =   min(box.right,        box.left);
                box.bottom = min(footprint.Height, box.bottom);
                box.top =    min(box.bottom,       box.top);
            }
        }
        else
        {
            // Get the mip level of the subresource
            const UINT mipLevel = DecomposeSubresourceIdxExtendedGetMip(ApiRequestedSubresource, pSrc->AppDesc()->MipLevels());
            const UINT baseMipLevel = DecomposeSubresourceIdxExtendedGetMip(ApiBaseSubresource, pSrc->AppDesc()->MipLevels());
            const UINT mipTransform = mipLevel - baseMipLevel;
            static_assert(D3D12_REQ_MIP_LEVELS < 32, "Bitshifting by number of mips should be fine for a UINT.");

            const UINT WidthAlignment = CD3D11FormatHelper::GetWidthAlignment(pSrc->AppDesc()->Format());
            const UINT HeightAlignment = CD3D11FormatHelper::GetHeightAlignment(pSrc->AppDesc()->Format());
            const UINT DepthAlignment = CD3D11FormatHelper::GetDepthAlignment(pSrc->AppDesc()->Format());

            // AlignAtLeast is chosen for right bounds to prevent bitshifting from resulting in empty rects.
            box.right =  min(footprint.Width,  AlignAtLeast(box.right  >> mipTransform, WidthAlignment));
            box.left =   min(box.right,        Align(       box.left   >> mipTransform, WidthAlignment));
            box.bottom = min(footprint.Height, AlignAtLeast(box.bottom >> mipTransform, HeightAlignment));
            box.top =    min(box.bottom,       Align(       box.top    >> mipTransform, HeightAlignment));
            box.back =   min(footprint.Depth,  AlignAtLeast(box.back   >> mipTransform, DepthAlignment));
            box.front =  min(box.back,         Align(       box.front  >> mipTransform, DepthAlignment));
        }
    }

    // This method should not generate empty rects.
    assert(box.left < box.right && box.top < box.bottom && box.front < box.back);
    return box;
}

D3D12_BOX ImmediateContext::GetBoxFromResource(Resource *pSrc, UINT SrcSubresource)
{
    return GetSubresourceBoxFromBox(pSrc, SrcSubresource, 0, CD3DX12_BOX(0, 0, 0, pSrc->AppDesc()->Width(), pSrc->AppDesc()->Height(), pSrc->AppDesc()->Depth()));
}

// Handles copies that are either:
// * A copy to/from the same subresource but at different offsets
// * A copy to/from suballocated resources that are both from the same underlying heap
void ImmediateContext::SameResourceCopy(Resource *pDst, UINT DstSubresource, Resource *pSrc, UINT SrcSubresource, UINT dstX, UINT dstY, UINT dstZ, const D3D12_BOX *pSrcBox)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Same Resource Copy");
#endif
    D3D12_BOX PatchedBox = {};
    if (!pSrcBox)
    {
        PatchedBox = GetBoxFromResource(pSrc, SrcSubresource);
        pSrcBox = &PatchedBox;
    }

    const bool bIsBoxEmpty = (pSrcBox->left >= pSrcBox->right || pSrcBox->top >= pSrcBox->bottom || pSrcBox->front >= pSrcBox->back);
    if (bIsBoxEmpty)
    {
        return;
    }

    // TODO: Profile the best strategy for handling same resource copies based on perf from games running on 9on12/11on12.
    // The default strategy is keep a per-context buffer that we re-use whenever we need to handle copies that require an intermediate 
    // buffer. The trade-off is that the GPU needs to swizzle-deswizzle when copying in and out of the resource. The alternative strategy is 
    // to instead allocate a resource everytime a same-resource copy is done but the intermediate resource will match the src/dst 
    // resource, avoiding any need to swizzle/deswizzle.
    // Task captured in VSO #7121286

    ResourceCreationArgs StagingResourceCreateArgs = {};
    D3D12_RESOURCE_DESC &StagingDesc = StagingResourceCreateArgs.m_desc12;
    StagingDesc = pSrc->Parent()->m_desc12;
    bool bUseBufferCopy = StagingDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;

    // Use pSrcBox to determine the minimal size we need to allocate for the staging resource
    UINT StagingHeight = StagingDesc.Height = pSrcBox->bottom - pSrcBox->top;
    StagingDesc.Width = pSrcBox->right - pSrcBox->left;
    UINT StagingWidth = static_cast<UINT>(StagingDesc.Width);
    UINT StagingDepth = StagingDesc.DepthOrArraySize = static_cast<UINT16>(pSrcBox->back - pSrcBox->front);

    // Query the footprint, this is necessary for resources that have varying formats per subresource (planar formats such as NV12)
    DXGI_FORMAT StagingFormat = StagingDesc.Format = bUseBufferCopy ? DXGI_FORMAT_UNKNOWN : pSrc->GetSubresourcePlacement(SrcSubresource).Footprint.Format;

    StagingDesc.MipLevels = 1;
    StagingDesc.Flags = (D3D12_RESOURCE_FLAGS)0;
    StagingResourceCreateArgs.m_appDesc = AppResourceDesc(0, 1, 1, 1, 1, StagingDepth, StagingWidth, StagingHeight, StagingFormat, StagingDesc.SampleDesc.Count,
        StagingDesc.SampleDesc.Quality, RESOURCE_USAGE_DEFAULT, (RESOURCE_CPU_ACCESS)0, (RESOURCE_BIND_FLAGS)0, StagingDesc.Dimension);

    UINT64 resourceSize = 0;
    m_pDevice12->GetCopyableFootprints(&StagingDesc, 0, 1, 0, nullptr, nullptr, nullptr, &resourceSize);
    bool bReallocateStagingBuffer = false;

    auto &pStagingResource = bUseBufferCopy ? m_pStagingBuffer : m_pStagingTexture;
    StagingDesc = CD3DX12_RESOURCE_DESC::Buffer(resourceSize);
    StagingResourceCreateArgs.m_isPlacedTexture = !bUseBufferCopy;
    if (!pStagingResource || pStagingResource->Parent()->m_heapDesc.SizeInBytes < resourceSize)
    {
        bReallocateStagingBuffer = true;
    }

    if (bReallocateStagingBuffer)
    {
        StagingResourceCreateArgs.m_heapDesc = CD3DX12_HEAP_DESC(resourceSize, GetHeapProperties(D3D12_HEAP_TYPE_DEFAULT));

        pStagingResource = Resource::CreateResource(this, StagingResourceCreateArgs, ResourceAllocationContext::ImmediateContextThreadLongLived);
    }
    else
    {
        pStagingResource->UpdateAppDesc(StagingResourceCreateArgs.m_appDesc);
    }
    assert(pStagingResource);

    const D3D12_BOX StagingSrcBox = CD3DX12_BOX(0, 0, 0, pSrcBox->right - pSrcBox->left, pSrcBox->bottom - pSrcBox->top, pSrcBox->back - pSrcBox->front);

    // Pick just one of the resources to call transitions on (don't need to transition both the src and dst since the underlying resource is the same),
    // we pick pDst since PostCopy will revert it back to COPY_SOURCE if it's an upload heap
    const UINT TransitionSubresource = DstSubresource;
    Resource *pTransitionResource = pDst;

    m_ResourceStateManager.TransitionResource(pStagingResource.get(), D3D12_RESOURCE_STATE_COPY_DEST);
    m_ResourceStateManager.TransitionSubresource(pTransitionResource, TransitionSubresource, D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_ResourceStateManager.ApplyAllResourceTransitions();
    CopyAndConvertSubresourceRegion(pStagingResource.get(), 0, pSrc, SrcSubresource, 0, 0, 0, reinterpret_cast<const D3D12_BOX*>(pSrcBox));

    m_ResourceStateManager.TransitionResource(pStagingResource.get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    m_ResourceStateManager.TransitionSubresource(pTransitionResource, TransitionSubresource, D3D12_RESOURCE_STATE_COPY_DEST);
    m_ResourceStateManager.ApplyAllResourceTransitions();
    CopyAndConvertSubresourceRegion(pDst, DstSubresource, pStagingResource.get(), 0, dstX, dstY, dstZ, reinterpret_cast<const D3D12_BOX*>(&StagingSrcBox));
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PostCopy(Resource *pSrc, UINT srcSubresource, Resource *pDst, UINT dstSubresource, UINT totalNumSubresources)
{
    bool bUsesSameUnderlyingSubresource = Resource::IsSameUnderlyingSubresource(pSrc, srcSubresource, pDst, dstSubresource);
#if DBG
    for (UINT i = 1; i < totalNumSubresources; i++)
    {
        assert(bUsesSameUnderlyingSubresource == Resource::IsSameUnderlyingSubresource(pSrc, srcSubresource + i, pDst, dstSubresource + i));
    }
#else
    UNREFERENCED_PARAMETER(totalNumSubresources);
#endif

    PostRender(COMMAND_LIST_TYPE::GRAPHICS);


    // Revert suballocated resource's owning heap back to the default state
    bool bResourceTransitioned = false;

    if (pSrc && !pSrc->GetIdentity()->m_bOwnsUnderlyingResource && pSrc->GetAllocatorHeapType() == AllocatorHeapType::Readback &&
        !bUsesSameUnderlyingSubresource) // Will automatically be transitioned back to COPY_DEST if this is part of a same resource copy
    {
        m_ResourceStateManager.TransitionResource(pSrc, GetDefaultPoolState(pSrc->GetAllocatorHeapType()));
        bResourceTransitioned = true;
    }

    if (pDst && !pDst->GetIdentity()->m_bOwnsUnderlyingResource && pDst->GetAllocatorHeapType() == AllocatorHeapType::Upload)
    {
        m_ResourceStateManager.TransitionResource(pDst, GetDefaultPoolState(pDst->GetAllocatorHeapType()));
        bResourceTransitioned = true;
    }

    if (bResourceTransitioned)
    {
        m_ResourceStateManager.ApplyAllResourceTransitions();
    }

#if DBG
    if (m_DebugFlags & Debug_FlushOnCopy && HasCommands(COMMAND_LIST_TYPE::GRAPHICS))
    {
        SubmitCommandList(COMMAND_LIST_TYPE::GRAPHICS);  // throws
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PostUpload()
{
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
#if DBG
    if (m_DebugFlags & Debug_FlushOnDataUpload && HasCommands(COMMAND_LIST_TYPE::GRAPHICS))
    {
        SubmitCommandList(COMMAND_LIST_TYPE::GRAPHICS);  // throws
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ConstantBufferBound(Resource* pBuffer, UINT slot, EShaderStage stage) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.ConstantBufferBound(stage, slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ConstantBufferUnbound(Resource* pBuffer, UINT slot, EShaderStage stage) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.ConstantBufferUnbound(stage, slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::VertexBufferBound(Resource* pBuffer, UINT slot) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.VertexBufferBound(slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::VertexBufferUnbound(Resource* pBuffer, UINT slot) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.VertexBufferUnbound(slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::IndexBufferBound(Resource* pBuffer) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.IndexBufferBound();
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::IndexBufferUnbound(Resource* pBuffer) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.IndexBufferUnbound();
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::StreamOutputBufferBound(Resource* pBuffer, UINT slot) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.StreamOutputBufferBound(slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::StreamOutputBufferUnbound(Resource* pBuffer, UINT slot) noexcept
{
    if (pBuffer == nullptr) return;
    pBuffer->m_currentBindings.StreamOutputBufferUnbound(slot);
    pBuffer->m_pParent->m_ResourceStateManager.TransitionResourceForBindings(pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::AddObjectToResidencySet(Resource *pResource, COMMAND_LIST_TYPE commandListType)
{
    m_CommandLists[(UINT)commandListType]->AddResourceToResidencySet(pResource);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::Flush(UINT commandListTypeMask)
{
#ifdef USE_PIX
    PIXSetMarker(0ull, L"Flush");
#endif
    bool bSubmitCommandList = false;
    m_ResourceStateManager.ApplyAllResourceTransitions();

    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i] && m_CommandLists[i]->HasCommands())
        {
            m_CommandLists[i]->SubmitCommandList();
            bSubmitCommandList = true;
        }
    }

    // Even if there are no commands, the app could have still done things like delete resources,
    // these are expected to be cleaned up on a per-flush basis
    PostSubmitNotification();
    return bSubmitCommandList;
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::PrepForCommandQueueSync(UINT commandListTypeMask)
{
    Flush(commandListTypeMask);
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i])
        {
            assert(!m_CommandLists[i]->HasCommands());
            m_CommandLists[i]->PrepForCommandQueueSync();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::IaSetTopology(D3D12_PRIMITIVE_TOPOLOGY topology )
{
    m_PrimitiveTopology = topology;
    GetGraphicsCommandList()->IASetPrimitiveTopology(m_PrimitiveTopology);
    m_StatesToReassert &= ~(e_PrimitiveTopologyDirty);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::IaSetVertexBuffers(UINT StartSlot, __in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT NumBuffers, Resource* const* pVBs, const UINT*pStrides, const UINT* pOffsets)
{
    // TODO: Partial bindings
    for (UINT i = 0; i < NumBuffers; ++i)
    {   
        UINT slot = i + StartSlot;
        Resource* pVB = pVBs[i];
        m_CurrentState.m_VBs.UpdateBinding(slot, pVB, e_Graphics);

        m_auVertexOffsets[slot] = pOffsets[i];
        m_auVertexStrides[slot] = pStrides[i];
    }
    // TODO: Track offsets to conditionally set this dirty bit
    m_DirtyStates |= e_VertexBuffersDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::IaSetIndexBuffer(Resource* pIB, DXGI_FORMAT fmt, UINT offset)
{
    m_CurrentState.m_IB.UpdateBinding(0, pIB, e_Graphics);
    m_IndexBufferFormat = fmt;
    m_uIndexBufferOffset = offset;

    // TODO: Track changes to format/offset to conditionally set this dirty bit
    m_DirtyStates |= e_IndexBufferDirty;
}


//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SoSetTargets(_In_range_(0, 4) UINT NumTargets, _In_range_(0, 4) UINT ClearSlots, _In_reads_(NumTargets) Resource* const* pBuffers, _In_reads_(NumTargets) const UINT* offsets )
{
    // TODO: Partial bindings
    bool bDirty = false;
    for (UINT i = 0; i < NumTargets; ++i)
    {
        UINT slot = i;
        Resource* pSOBuffer = pBuffers[i];

        if ((offsets[i] != UINT_MAX) && pSOBuffer)
        {
            // Copy the new offset into the hidden BufferFilledSize
            assert(0 == offsetof(SStreamOutputSuffix, BufferFilledSize));

            UINT OffsetToBufferFilledSize = pSOBuffer->GetOffsetToStreamOutputSuffix();

            D3D12_BOX DstBox = 
            {
                OffsetToBufferFilledSize,
                0,
                0,
                OffsetToBufferFilledSize + sizeof(UINT),
                1,
                1
            };

            D3D11_SUBRESOURCE_DATA Data = {&offsets[i]};
            UpdateSubresources(pSOBuffer, CSubresourceSubset(CBufferView()), &Data, &DstBox, UpdateSubresourcesFlags::ScenarioImmediateContextInternalOp);
        }

        bDirty |= m_CurrentState.m_SO.UpdateBinding(slot, pSOBuffer, e_Graphics);
    }
    for (UINT i = NumTargets; i < NumTargets + ClearSlots; ++i)
    {
        bDirty |= m_CurrentState.m_SO.UpdateBinding(i, nullptr, e_Graphics);
    }
    m_DirtyStates |= bDirty ? e_StreamOutputDirty : 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::OMSetRenderTargets(__in_ecount(NumRTVs) RTV* const* ppRTVs, __in_range(0, 8) UINT NumRTVs, __in_opt DSV *pDSV)
{
    bool bDirtyRTVs = false;
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        auto pRTV = i < NumRTVs ? ppRTVs[i] : nullptr;
        if (m_CurrentState.m_RTVs.UpdateBinding(i, pRTV, e_Graphics))
        {
            bDirtyRTVs = true;
        }
    }

    if (m_CurrentState.m_DSVs.UpdateBinding(0, pDSV, e_Graphics))
    {
        bDirtyRTVs = true; // RTVs and DSV are updated together
    }
    m_DirtyStates |= bDirtyRTVs ? e_RenderTargetsDirty : 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::OMSetUnorderedAccessViews(UINT Start, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV* const* ppUAVs, __in_ecount(NumViews) CONST UINT* pInitialCounts )
{
    for (UINT i = 0; i < NumViews; ++i)
    {
        UINT slot = i + Start;
        UAV* pUAV = ppUAVs[i];

        // Ensure a counter resource is allocated for the UAV if necessary
        if(pUAV)
        {
            pUAV->EnsureCounterResource(); // throw( _com_error )
        }

        if ((pInitialCounts[i] != UINT_MAX) && pUAV)
        {
            pUAV->UpdateCounterValue(pInitialCounts[i]);
        }

        m_CurrentState.m_UAVs.UpdateBinding(slot, pUAV, e_Graphics);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CsSetUnorderedAccessViews(UINT Start, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV* const* ppUAVs, __in_ecount(NumViews) CONST UINT* pInitialCounts)
{
    for (UINT i = 0; i < NumViews; ++i)
    {
        UINT slot = i + Start;
        UAV* pUAV = ppUAVs[i];

        // Ensure a counter resource is allocated for the UAV if necessary
        if (pUAV)
        {
            pUAV->EnsureCounterResource(); // throw( _com_error )
        }

        if ((pInitialCounts[i] != UINT_MAX) && pUAV)
        {
            pUAV->UpdateCounterValue(pInitialCounts[i]);
        }

        m_CurrentState.m_CSUAVs.UpdateBinding(slot, pUAV, e_Compute);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::OMSetStencilRef(UINT StencilRef )
{
    m_uStencilRef = StencilRef;
    GetGraphicsCommandList()->OMSetStencilRef(StencilRef);
    m_StatesToReassert &= ~(e_StencilRefDirty);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
    memcpy(m_BlendFactor, BlendFactor, sizeof(m_BlendFactor));
    GetGraphicsCommandList()->OMSetBlendFactor(BlendFactor);
    m_StatesToReassert &= ~(e_BlendFactorDirty);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetViewport(UINT slot, const D3D12_VIEWPORT* pViewport)
{
    m_aViewports[slot] = *pViewport;
    m_StatesToReassert |= e_ViewportsDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetNumViewports(UINT num)
{
    m_uNumViewports = num;
    m_StatesToReassert |= e_ViewportsDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetScissorRect(UINT slot, const D3D12_RECT* pRect )
{
    m_aScissors[slot] = *pRect;
    m_StatesToReassert |= e_ScissorRectsDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetNumScissorRects(UINT num)
{
    m_uNumScissors = num;
    m_StatesToReassert |= e_ScissorRectsDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetScissorRectEnable(BOOL ScissorRectEnable)
{
    if (m_ScissorRectEnable != ScissorRectEnable)
    {
        m_ScissorRectEnable = ScissorRectEnable;
        m_StatesToReassert |= e_ScissorRectsDirty;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearRenderTargetView(RTV *pRTV, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    TransitionResourceForView(pRTV, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto Descriptor = pRTV->GetRefreshedDescriptorHandle();
    GetGraphicsCommandList()->ClearRenderTargetView(Descriptor, color, NumRects, pRects);
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}


//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearDepthStencilView(DSV *pDSV, UINT Flags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
    if (!Flags)
    {
        return;
    }
    
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    if (Flags == 0)
    {
        // The runtime guarantees that clear flags won't cause clears on read-only planes of the view,
        // but doesn't drop the call if no clears are happening
        return;
    }

    {
        D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = pDSV->GetDesc12();
        DSVDesc.Flags = (D3D12_DSV_FLAGS)((((Flags & D3D11_CLEAR_DEPTH) == 0) ? D3D12_DSV_FLAG_READ_ONLY_DEPTH : 0) |
                                          (((Flags & D3D11_CLEAR_STENCIL) == 0) ? D3D12_DSV_FLAG_READ_ONLY_STENCIL : 0));
        CViewSubresourceSubset ViewSubresources(DSVDesc,
                                                pDSV->m_pResource->AppDesc()->MipLevels(),
                                                pDSV->m_pResource->AppDesc()->ArraySize(),
                                                pDSV->m_pResource->SubresourceMultiplier(),
                                                CViewSubresourceSubset::WriteOnly);
        assert(!ViewSubresources.IsEmpty());
        m_ResourceStateManager.TransitionSubresources(pDSV->m_pResource, ViewSubresources, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
    m_ResourceStateManager.ApplyAllResourceTransitions();

    static_assert(D3D11_CLEAR_DEPTH == D3D12_CLEAR_FLAG_DEPTH, "Casting flags");
    static_assert(D3D11_CLEAR_STENCIL == D3D12_CLEAR_FLAG_STENCIL, "Casting flags");
    auto Descriptor = pDSV->GetRefreshedDescriptorHandle();
    GetGraphicsCommandList()->ClearDepthStencilView(Descriptor, static_cast<D3D12_CLEAR_FLAGS>(Flags), Depth, Stencil, NumRects, pRects);
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearUnorderedAccessViewUint(UAV *pUAV, CONST UINT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    pUAV->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
    TransitionResourceForView(pUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto Descriptor = pUAV->GetRefreshedDescriptorHandle();
    UINT ViewHeapSlot = ReserveSlots(m_ViewHeap, 1); // throw( _com_error )
    D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = m_ViewHeap.GPUHandle(ViewHeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = m_ViewHeap.CPUHandle(ViewHeapSlot);

    m_pDevice12->CopyDescriptorsSimple( 1, CPUDescriptor, Descriptor, m_ViewHeap.m_Desc.Type );

    GetGraphicsCommandList()->ClearUnorderedAccessViewUint(GPUDescriptor, Descriptor, pUAV->m_pResource->GetUnderlyingResource(),
                                                        color, NumRects, pRects);

    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearUnorderedAccessViewFloat(UAV *pUAV, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    pUAV->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
    TransitionResourceForView(pUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto Descriptor = pUAV->GetRefreshedDescriptorHandle();

    UINT ViewHeapSlot = ReserveSlots(m_ViewHeap, 1); // throw( _com_error )
    D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = m_ViewHeap.GPUHandle(ViewHeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = m_ViewHeap.CPUHandle(ViewHeapSlot);

    m_pDevice12->CopyDescriptorsSimple( 1, CPUDescriptor, Descriptor, m_ViewHeap.m_Desc.Type );

    GetGraphicsCommandList()->ClearUnorderedAccessViewFloat(GPUDescriptor, Descriptor, pUAV->m_pResource->GetUnderlyingResource(),
                                                            color, NumRects, pRects);

    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

template <typename T> T FloatTo(float x, T max = std::numeric_limits<T>::max())
{
    return x != x ? (T)0 : (max > x ? (T)x : max);
}

//----------------------------------------------------------------------------------------------------------------------------------
// If a resource doesn't have a render target, we have to create a temp resource of identical format, clear the temp resource, and then 
// copy it's contents into this resource. The ResourceCache caches resources to ensure that we pool these temp resources and make future
// clears fast.
void TRANSLATION_API ImmediateContext::ClearResourceWithNoRenderTarget(Resource* pResource, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects, UINT Subresource, UINT BaseSubresource, DXGI_FORMAT clearFormat)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Clearing resource via copy");
#endif
    assert(CD3D11FormatHelper::GetTypeLevel(clearFormat) == D3D11FTL_FULL_TYPE);
    auto& Footprint = pResource->GetSubresourcePlacement(Subresource).Footprint;

    // The color we receive is intended for a specific plane. If we're clearing a different plane, we need to adjust the color.
    // ClearRenderTargetView always expects 4 colors, and we can shift at most two channels, so prep 6 colors. The values of the last two don't matter.
    // For most planar surfaces, we'll shift once so R goes to R for the Y plane, and GB goes to RG for the UV plane.
    // For 3-plane surfaces, we'll send one color to each plane.
    float adjustedColor[6] = {color[0], color[1], color[2], color[3], 0.0f, 0.0f};
    UINT BasePlane = GetPlaneIdxFromSubresourceIdx(BaseSubresource, pResource->AppDesc()->SubresourcesPerPlane());
    UINT TargetPlane = GetPlaneIdxFromSubresourceIdx(Subresource, pResource->AppDesc()->SubresourcesPerPlane());
    assert(BasePlane <= TargetPlane);
    color = adjustedColor + (TargetPlane - BasePlane);

    RECT resourceRect = {};
    resourceRect.right = Footprint.Width;
    resourceRect.bottom = Footprint.Height;

    const RECT *pCopyRects = &resourceRect;
    UINT numCopyRects = 1;
    if (NumRects)
    {
        auto& BaseFootprint = pResource->GetSubresourcePlacement(BaseSubresource).Footprint;
        m_RectCache.clear();
        m_RectCache.reserve(NumRects);
        for (UINT i = 0; i < NumRects; ++i)
        {
            if (pRects[i].right <= pRects[i].left ||
                pRects[i].bottom <= pRects[i].top ||
                pRects[i].right <= 0 ||
                pRects[i].bottom <= 0 ||
                pRects[i].left >= static_cast<LONG>(BaseFootprint.Width) ||
                pRects[i].top >= static_cast<LONG>(BaseFootprint.Height))
            {
                // Drop empty rects, because GetSubresourceBoxFromBox wants to ensure that decreasing size due to
                // mip levels doesn't return zeroes, but empty rects will have zeroes.
                continue;
            }

            D3D12_BOX Box = GetSubresourceBoxFromBox(pResource, Subresource, BaseSubresource,
                CD3DX12_BOX(max(0l, pRects[i].left), max(0l, pRects[i].top), max(0l, pRects[i].right), max(0l, pRects[i].bottom)));
            m_RectCache.push_back( CD3DX12_RECT(Box.left, Box.top, Box.right, Box.bottom) );
        }

        pCopyRects = m_RectCache.data();
        numCopyRects = static_cast<UINT>(m_RectCache.size());
    }
    if (numCopyRects == 0)
    {
        // All rects were empty, no-op this call.
        return;
    }

    if (!SupportsRenderTarget(clearFormat))
    {
        assert(!CD3D11FormatHelper::Planar(Footprint.Format)); // Each plane should have a a different non-planar format.
        BYTE ClearColor[16] = {};
        // Note: Staging layout for all YUV formats is RGBA, even though the channels
        // mapped through views are potentially out-of-order.
        // Comments on channel mappings taken straight from MSDN.
        switch (Footprint.Format)
        {
        case DXGI_FORMAT_YUY2: // 8bit 4:2:2
        {
            assert(CD3D11FormatHelper::GetByteAlignment(Footprint.Format) == 4);
            ClearColor[0] = FloatTo<BYTE>(color[0]); // Y0 -> R8
            ClearColor[1] = FloatTo<BYTE>(color[1]); // U0 -> G8
            ClearColor[2] = FloatTo<BYTE>(color[0]); // Y1 -> B8
            ClearColor[3] = FloatTo<BYTE>(color[2]); // V0 -> A8
            break;
        }
        case DXGI_FORMAT_Y210: // both represented as 16bit 4:2:2
        case DXGI_FORMAT_Y216:
        {
            assert(CD3D11FormatHelper::GetByteAlignment(Footprint.Format) == 8);
            const USHORT maxValue = Footprint.Format == DXGI_FORMAT_Y210 ? (USHORT)((1 << 10) - 1) : (USHORT)((1 << 16) - 1);
            USHORT* pClearColor16bit = reinterpret_cast<USHORT*>(ClearColor);
            pClearColor16bit[0] = FloatTo<USHORT>(color[0], maxValue); // Y0 -> R16
            pClearColor16bit[1] = FloatTo<USHORT>(color[1], maxValue); // U0 -> G16
            pClearColor16bit[2] = FloatTo<USHORT>(color[0], maxValue); // Y1 -> B16
            pClearColor16bit[3] = FloatTo<USHORT>(color[2], maxValue); // V0 -> A16
            break;
        }
        case DXGI_FORMAT_Y416: // 16bit 4:4:4
        {
            assert(CD3D11FormatHelper::GetByteAlignment(Footprint.Format) == 8);
            USHORT* pClearColor16bit = reinterpret_cast<USHORT*>(ClearColor);
            pClearColor16bit[0] = FloatTo<USHORT>(color[1]); // U -> R16
            pClearColor16bit[1] = FloatTo<USHORT>(color[0]); // Y -> G16
            pClearColor16bit[2] = FloatTo<USHORT>(color[2]); // V -> B16
            pClearColor16bit[3] = FloatTo<USHORT>(color[3]); // A -> A16
            break;
        }
        case DXGI_FORMAT_Y410: // 10bit 4:4:4, packed into R10G10B10A2
        {
            assert(CD3D11FormatHelper::GetByteAlignment(Footprint.Format) == 4);
            const UINT maxValue = (1 << 10) - 1;
            UINT ClearColor1010102 = (FloatTo<UINT>(color[1], maxValue)) |       // U -> R10
                                     (FloatTo<UINT>(color[0], maxValue) << 10) | // Y -> G10
                                     (FloatTo<UINT>(color[2], maxValue) << 20) | // V -> B10
                                     (FloatTo<UINT>(color[3], 3) << 30);         // A -> A2
            *reinterpret_cast<UINT*>(ClearColor) = ClearColor1010102;
            break;
        }
        default:
            assert(false);
            return; // No-op the clear.
        }

        UINT Mip = 0, ArraySlice = 0, PlaneSlice = 0;
        pResource->DecomposeSubresource(Subresource, Mip, ArraySlice, PlaneSlice);
        CSubresourceSubset SingleSubresourceSubset(1, 1, 1, (UINT8)Mip, (UINT16)ArraySlice);

        for (UINT i = 0; i < numCopyRects; ++i)
        {
            D3D12_BOX srcBox = CD3DX12_BOX(pCopyRects[i].left, pCopyRects[i].top, pCopyRects[i].right, pCopyRects[i].bottom);
            UpdateSubresources(pResource,
                               SingleSubresourceSubset,
                               nullptr,
                               &srcBox,
                               UpdateSubresourcesFlags::ScenarioImmediateContext,
                               ClearColor);
        }
    }
    else
    {
        // The target resource does not support render target, but the format can support render target.
        // Create a temporary resource to clear, and copy to the target resource.
        // The temporary resource has a max size and tiled copies are performed if needed.

        // Get the 64K tile shape and scale it up to about 1MB.  This is done to compensate for 
        // command submission overhead and can be tuned.  
        D3D11_TILE_SHAPE tileShape = {};
        CD3D11FormatHelper::GetTileShape(&tileShape, clearFormat, D3D11_RESOURCE_DIMENSION_TEXTURE2D, 1);
        tileShape.WidthInTexels *= 4;
        tileShape.HeightInTexels *= 4;

        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;
        FLOAT ClearColor[4] = { color[0], color[1], color[2], color[3] };

        switch (clearFormat)
        {
        case DXGI_FORMAT_AYUV:
            viewFormat = DXGI_FORMAT_R8G8B8A8_UINT;
            ClearColor[0] = color[2]; // V8 -> R8
            ClearColor[1] = color[1]; // U8 -> G8
            ClearColor[2] = color[0]; // Y8 -> B8
            ClearColor[3] = color[3]; // A8 -> A8
            break;
        }


        // Request a resource with the tile size.
        auto& CacheEntry = GetResourceCache().GetResource(clearFormat, tileShape.WidthInTexels, tileShape.HeightInTexels, viewFormat);

        // The resource cache returns a resource that is the requested size or larger.
        tileShape.WidthInTexels = CacheEntry.m_Resource->AppDesc()->Width();
        tileShape.HeightInTexels = CacheEntry.m_Resource->AppDesc()->Height();

        // Find the largest rectangle that must be cleared.
        D3D12_RECT clearRect = pCopyRects[0];
        for (UINT i = 1; i < numCopyRects; ++i)
        {
            UnionRect(&clearRect, &clearRect, &pCopyRects[i]);
        }

        // Translate that rectangle to 0,0
        clearRect.right = clearRect.right - clearRect.left;
        clearRect.left = 0;
        clearRect.bottom = clearRect.bottom - clearRect.top;
        clearRect.top = 0;

        // Find the minimum region to clear in the tile.  This is smaller than the full tile when
        // the destination rectangles are smaller than the tile size.
        {
            D3D12_RECT clearResourceRect = CD3DX12_RECT(0, 0, tileShape.WidthInTexels, tileShape.HeightInTexels);
            IntersectRect(&clearRect, &clearRect, &clearResourceRect);
        }

        // Clear the region needed in the allocated resource.
        ClearRenderTargetView(CacheEntry.m_RTV.get(), ClearColor, 1, &clearRect);

        // Loop over the rects to clear the region.
        for (UINT i = 0; i < numCopyRects; ++i)
        {
            D3D12_RECT copyRect = pCopyRects[i];

            // Loop over tile rows of the source.
            while (copyRect.top < copyRect.bottom)
            {
                // Construct the source rect to copy.  Clamp to account for the bottom edge of the destination.
                D3D12_RECT srcTileCopyRect = CD3DX12_RECT(clearRect.left, clearRect.top, 0, 0);
                srcTileCopyRect.bottom = srcTileCopyRect.top + std::min(clearRect.bottom - clearRect.top, copyRect.bottom - copyRect.top);

                copyRect.left = pCopyRects[i].left;

                // Loop over tile columns of this row.
                while (copyRect.left < copyRect.right)
                {
                    // Clamp to the right edge of the destination.
                    srcTileCopyRect.right = srcTileCopyRect.left + std::min(clearRect.right - clearRect.left, copyRect.right - copyRect.left);

                    // Copy to the target to clear it.
                    D3D12_BOX srcBox = CD3DX12_BOX(srcTileCopyRect.left, srcTileCopyRect.top, srcTileCopyRect.right, srcTileCopyRect.bottom);
                    ResourceCopyRegion(
                        pResource,
                        Subresource,
                        copyRect.left,
                        copyRect.top,
                        0,
                        CacheEntry.m_Resource.get(),
                        0,
                        &srcBox);

                    // Advanced to the next tile column.
                    copyRect.left += srcTileCopyRect.right - srcTileCopyRect.left;
                }

                // Advanced to the next row.
                copyRect.top += srcTileCopyRect.bottom - srcTileCopyRect.top;
            }
        }
    }
}

template <typename TIface>
void ImmediateContext::ClearViewWithNoRenderTarget(View<TIface>* pView, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    Resource *pResource = pView->m_pResource;
    for (auto range : pView->m_subresources)
    {
        for (UINT subresource = range.first; subresource < range.second; ++subresource)
        {
            DXGI_FORMAT format = pResource->GetSubresourcePlacement(subresource).Footprint.Format;

            // We should always have a full type at this point, since the resource cache will fail to create RTVs otherwise.
            // We should only get this far on fully typed resources, but we'll need to patch up planar resources,
            // as the footprint ends up with a typeless format for each plane. Currently we only support 8 and 16 bit
            // planar, with one or two channels per plane.
            switch (format)
            {
            case DXGI_FORMAT_R8_TYPELESS:
                format = DXGI_FORMAT_R8_UINT;
                break;
            case DXGI_FORMAT_R8G8_TYPELESS:
                format = DXGI_FORMAT_R8G8_UINT;
                break;
            case DXGI_FORMAT_R16_TYPELESS:
                format = DXGI_FORMAT_R16_UINT;
                break;
            case DXGI_FORMAT_R16G16_TYPELESS:
                format = DXGI_FORMAT_R16G16_UINT;
                break;
            }

            assert(format == pResource->AppDesc()->Format() || CD3D11FormatHelper::Planar(pResource->AppDesc()->Format()));
            ClearResourceWithNoRenderTarget(pResource, color, NumRects, pRects, subresource, pView->m_subresources.begin().StartSubresource(), format);
        }
    }

    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearVideoDecoderOutputView(VDOV *pVDOV, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    ClearViewWithNoRenderTarget(pVDOV, color, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearVideoProcessorInputView(VPIV *pVPIV, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    ClearViewWithNoRenderTarget(pVPIV, color, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearVideoProcessorOutputView(VPOV *pVPOV, CONST FLOAT color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    ClearViewWithNoRenderTarget(pVPOV, color, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Note: no resource transitions occur as a result of discard
void TRANSLATION_API ImmediateContext::DiscardView(ViewBase* pView, const D3D12_RECT* pRects, UINT NumRects)
{
    UINT commandListTypeMask = pView->m_pResource->GetCommandListTypeMask(pView->m_subresources);

    if (commandListTypeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
    {
        // TODO: output a no-op msg for this case
        return;
    }

    if (pView->m_pResource->Parent()->ResourceDimension12() != D3D12_RESOURCE_DIMENSION_TEXTURE2D && NumRects)
    {
        // D3D12 will treat this as invalid
        // Since this call is just a hint anyway, drop the call
        return;
    }

    // D3D12 requires RenderTargets and DepthStenciles to be transitioned to the corresponding write state before discard.
    auto pAPIResource = GetUnderlyingResource(pView->m_pResource);
    D3D12_RESOURCE_FLAGS ResourceFlags = pAPIResource->GetDesc().Flags;

    if ((ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
    {
        D3D12_RESOURCE_STATES RequiredState =
            (ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;

        TransitionResourceForView(pView, RequiredState);
        m_ResourceStateManager.ApplyAllResourceTransitions();
    }

    // TODO: Tokenize Discard operations and perform them just-in-time before future render ops
    // to ensure they happen on the same command list as the one doing the op.

    bool allSubresourcesSame = IsSingleCommandListType(commandListTypeMask);
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if (commandListTypeMask & (1 << i))
        {
            DiscardViewImpl((COMMAND_LIST_TYPE)i, pView, pRects, NumRects, allSubresourcesSame);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::DiscardViewImpl(COMMAND_LIST_TYPE commandListType, ViewBase* pView, const D3D12_RECT* pRects, UINT NumRects, bool allSubresourcesSame)
{
    PreRender(commandListType);
    D3D12_DISCARD_REGION Desc;
    Desc.pRects = reinterpret_cast<const D3D12_RECT*>(pRects);
    Desc.NumRects = NumRects;

    auto pAPIResource = GetUnderlyingResource(pView->m_pResource);
    pView->m_pResource->UsedInCommandList(commandListType, GetCommandListID(commandListType));

    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "ImmediateContext::DiscardView must support all command list types.");

    auto pfnDiscardResource = [&]()
    {
        switch (commandListType)
        {
        case COMMAND_LIST_TYPE::GRAPHICS:
            GetGraphicsCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        case COMMAND_LIST_TYPE::VIDEO_DECODE:
            GetVideoDecodeCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            GetVideoProcessCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        }
    };

    if (allSubresourcesSame)
    {
        for (auto range : pView->m_subresources)
        {
            Desc.FirstSubresource = range.first;
            Desc.NumSubresources = range.second - range.first;
            pfnDiscardResource();
        }
    }
    else
    {
        // need to discard on a per-subresource basis
        Desc.NumSubresources = 1;
        for (auto range : pView->m_subresources)
        {
            for (UINT subResource = range.first; subResource < range.second; subResource++)
            {
                if (pView->m_pResource->GetCommandListTypeMask(subResource) & (UINT)commandListType)
                {
                    Desc.FirstSubresource = subResource;
                    pfnDiscardResource();
                }
            }
        }
    }

    PostRender(commandListType);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Note: no resource transitions occur as a result of discard
void TRANSLATION_API ImmediateContext::DiscardResource(Resource* pResource, const D3D12_RECT* pRects, UINT NumRects)
{
    UINT commandListTypeMask = pResource->GetCommandListTypeMask();
    if (commandListTypeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
    {
        // TODO: output a no-op msg for this case
        return;
    }

    if (pResource->Parent()->ResourceDimension12() != D3D12_RESOURCE_DIMENSION_TEXTURE2D && NumRects)
    {
        // D3D12 will treat this as invalid
        // Since this call is just a hint anyway, drop the call
        return;
    }

    auto pAPIResource = GetUnderlyingResource(pResource);

    // D3D12 requires RenderTargets and DepthStenciles to be transitioned to the corresponding write state before discard.
    D3D12_RESOURCE_FLAGS ResourceFlags = pAPIResource->GetDesc().Flags;
    if ((ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
    {
        D3D12_RESOURCE_STATES RequiredState =
            (ResourceFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE;

        m_ResourceStateManager.TransitionResource(pResource, RequiredState);
        m_ResourceStateManager.ApplyAllResourceTransitions();
    }

    // TODO: Tokenize Discard operations and perform them just-in-time before future render ops
    // to ensure they happen on the same command list as the one doing the op.

    bool allSubresourcesSame = IsSingleCommandListType(commandListTypeMask);
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if (commandListTypeMask & (1 << i))
        {
            DiscardResourceImpl((COMMAND_LIST_TYPE)i, pResource, pRects, NumRects, allSubresourcesSame);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::DiscardResourceImpl(COMMAND_LIST_TYPE commandListType, Resource* pResource, const D3D12_RECT* pRects, UINT NumRects, bool allSubresourcesSame)
{
    PreRender(commandListType);
    D3D12_DISCARD_REGION Desc;
    Desc.pRects = reinterpret_cast<const D3D12_RECT*>(pRects);
    Desc.NumRects = NumRects;

    auto pAPIResource = GetUnderlyingResource(pResource);

    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "ImmediateContext::DiscardResource must support all command list types.");

    auto pfnDiscardResource = [&]()
    {
        switch (commandListType)
        {
        case COMMAND_LIST_TYPE::GRAPHICS:
            GetGraphicsCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        case COMMAND_LIST_TYPE::VIDEO_DECODE:
            GetVideoDecodeCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            GetVideoProcessCommandList()->DiscardResource(pAPIResource, &Desc);
            break;
        }
    };

    if (allSubresourcesSame)
    {
        Desc.FirstSubresource = 0;
        Desc.NumSubresources = pResource->NumSubresources();
        pfnDiscardResource();
    }
    else
    {
        Desc.NumSubresources = 1;
        for (UINT subResource = 0; subResource < pResource->NumSubresources(); subResource++)
        {
            if (pResource->GetCommandListTypeMask(subResource) & (UINT)commandListType)
            {
                Desc.FirstSubresource = subResource;
                pfnDiscardResource();
            }
        }
    }

    PostRender(commandListType);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::EnsureInternalUAVRootSig() noexcept(false)
{
    if (!m_InternalUAVRootSig.Created())
    {
        CD3DX12_DESCRIPTOR_RANGE1 UAVSlot;
        UAVSlot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER1 RootParams[2];
        RootParams[0].InitAsDescriptorTable(1, &UAVSlot);
        RootParams[1].InitAsConstants(NUM_UAV_ROOT_SIG_CONSTANTS, 0);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSigDesc(2, RootParams);
        m_InternalUAVRootSig.Create(RootSigDesc);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::EnsureDrawAutoResources() noexcept(false)
{
    EnsureInternalUAVRootSig(); // throw (_com_error);

    if (!m_pDrawAutoPSO)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc;
        ZeroMemory(&PSODesc, sizeof(PSODesc));

        PSODesc.pRootSignature = m_InternalUAVRootSig.GetRootSignature();
        PSODesc.CS.pShaderBytecode = g_DrawAutoCS;
        PSODesc.CS.BytecodeLength = sizeof(g_DrawAutoCS);
        PSODesc.NodeMask = GetNodeMask();

        HRESULT hr = m_pDevice12->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_pDrawAutoPSO));
        ThrowFailure(hr); // throw( _com_error )
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::EnsureQueryResources() noexcept(false)
{
    EnsureInternalUAVRootSig(); // throw (_com_error);

    if (!m_pFormatQueryPSO)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc;
        ZeroMemory(&PSODesc, sizeof(PSODesc));

        PSODesc.pRootSignature = m_InternalUAVRootSig.GetRootSignature();
        PSODesc.CS.pShaderBytecode = g_FormatQueryCS;
        PSODesc.CS.BytecodeLength = sizeof(g_FormatQueryCS);
        PSODesc.NodeMask = GetNodeMask();

        HRESULT hr = m_pDevice12->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_pFormatQueryPSO));
        ThrowFailure(hr); // throw( _com_error )
    }

    if (!m_pAccumulateQueryPSO)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc;
        ZeroMemory(&PSODesc, sizeof(PSODesc));

        PSODesc.pRootSignature = m_InternalUAVRootSig.GetRootSignature();
        PSODesc.CS.pShaderBytecode = g_AccumulateQueryCS;
        PSODesc.CS.BytecodeLength = sizeof(g_AccumulateQueryCS);
        PSODesc.NodeMask = GetNodeMask();

        HRESULT hr = m_pDevice12->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_pAccumulateQueryPSO));
        ThrowFailure(hr); // throw( _com_error )
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::EnsureExecuteIndirectResources() noexcept(false)
{
    HRESULT hr = S_OK;

    if (!m_pDrawInstancedCommandSignature)
    {
        D3D12_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc = {};
        CommandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
        CommandSignatureDesc.NumArgumentDescs = 1;
        CommandSignatureDesc.pArgumentDescs = &IndirectArg;
        CommandSignatureDesc.NodeMask = GetNodeMask();

        hr = m_pDevice12->CreateCommandSignature(
            &CommandSignatureDesc,
            nullptr,
            IID_PPV_ARGS(&m_pDrawInstancedCommandSignature)
            );

        ThrowFailure(hr);
    }

    if (!m_pDrawIndexedInstancedCommandSignature)
    {
        D3D12_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc = {};
        CommandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        CommandSignatureDesc.NumArgumentDescs = 1;
        CommandSignatureDesc.pArgumentDescs = &IndirectArg;
        CommandSignatureDesc.NodeMask = GetNodeMask();

        hr = m_pDevice12->CreateCommandSignature(
            &CommandSignatureDesc,
            nullptr,
            IID_PPV_ARGS(&m_pDrawIndexedInstancedCommandSignature)
            );

        ThrowFailure(hr);
    }

    if (!m_pDispatchCommandSignature)
    {
        D3D12_INDIRECT_ARGUMENT_DESC IndirectArg = {};
        IndirectArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc = {};
        CommandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        CommandSignatureDesc.NumArgumentDescs = 1;
        CommandSignatureDesc.pArgumentDescs = &IndirectArg;
        CommandSignatureDesc.NodeMask = GetNodeMask();

        hr = m_pDevice12->CreateCommandSignature(
            &CommandSignatureDesc,
            nullptr,
            IID_PPV_ARGS(&m_pDispatchCommandSignature)
            );

        ThrowFailure(hr);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
ID3D12PipelineState* ImmediateContext::PrepareGenerateMipsObjects(DXGI_FORMAT Format, D3D12_RESOURCE_DIMENSION Dimension) noexcept(false)
{
    MipGenKey Key( Format, Dimension );
    auto iter = m_pGenerateMipsPSOMap.find(Key);
    if (iter != m_pGenerateMipsPSOMap.end())
    {
        return iter->second.get();
    }

    HRESULT hr = S_OK;
    if (!m_GenerateMipsRootSig.Created())
    {
        // GenMips uses a custom RootSig to allow binding constants without needing to burn heap space for constant buffers
        // Total bindings: One SRV (the GenMips SRV), two root constants, and one sampler
        D3D12_SAMPLER_DESC SamplerDesc{};
        SamplerDesc.MinLOD = 0;
        SamplerDesc.MaxLOD = 9999.0f;
        SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        SamplerDesc.MipLODBias = 0;
        SamplerDesc.MaxAnisotropy = 1;
        SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

        auto pfnInitializeSampler = [=](D3D12_FILTER_TYPE FilterType, D3D12_FILTER SamplerFilter, D3D12_SAMPLER_DESC& SamplerDesc)
        {
            SamplerDesc.Filter = SamplerFilter;
            m_GenerateMipsSamplers[FilterType] = m_SamplerAllocator.AllocateHeapSlot(); // throw( _com_error )
            m_pDevice12->CreateSampler(&SamplerDesc, m_GenerateMipsSamplers[FilterType]);
        };
        
        pfnInitializeSampler(D3D12_FILTER_TYPE_LINEAR, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, SamplerDesc);
        pfnInitializeSampler(D3D12_FILTER_TYPE_POINT, D3D12_FILTER_MIN_MAG_MIP_POINT, SamplerDesc);

        CD3DX12_DESCRIPTOR_RANGE1 SRVSlot;
        SRVSlot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_DESCRIPTOR_RANGE1 SamplerSlot;
        SamplerSlot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

        CD3DX12_ROOT_PARAMETER1 RootParams[3];
        RootParams[GenerateMipsRootSignatureSlots::eSRV].InitAsDescriptorTable(1, &SRVSlot);
        RootParams[GenerateMipsRootSignatureSlots::eRootConstants].InitAsConstants(3, 0);
        RootParams[GenerateMipsRootSignatureSlots::eSampler].InitAsDescriptorTable(1, &SamplerSlot);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootSigDesc(_countof(RootParams), RootParams);
        m_GenerateMipsRootSig.Create(RootSigDesc);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
    PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PSODesc.DepthStencilState.DepthEnable = FALSE;
    PSODesc.pRootSignature = m_GenerateMipsRootSig.GetRootSignature();
    PSODesc.VS = { g_GenMipsVS, sizeof(g_GenMipsVS) };
    switch(Dimension)
    {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: PSODesc.PS = { g_GenMipsPS1D, sizeof(g_GenMipsPS1D) }; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D: PSODesc.PS = { g_GenMipsPS2D, sizeof(g_GenMipsPS2D) }; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: PSODesc.PS = { g_GenMipsPS3D, sizeof(g_GenMipsPS3D) }; break;
        default: ASSUME(false);
    }
    PSODesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    PSODesc.RTVFormats[0] = Format;
    PSODesc.NumRenderTargets = 1;
    PSODesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;
    PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PSODesc.SampleDesc.Count = 1;
    PSODesc.SampleMask = 0xffffffff;
    PSODesc.NodeMask = GetNodeMask();

    unique_comptr<ID3D12PipelineState> spPSO;
    hr = m_pDevice12->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&spPSO));
    ThrowFailure(hr); // throw( _com_error )

    auto insertRet = m_pGenerateMipsPSOMap.emplace(Key, std::move(spPSO));
    iter = insertRet.first;
    return iter->second.get();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::GenMips( SRV *pSRV, D3D12_FILTER_TYPE FilterType)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"GenerateMips");
#endif
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);
    auto pResource = pSRV->m_pResource;
    
    // GenerateMips is deprecated in D3D12
    // It is implemented in 11on12 via draws which average the pixels of the higher mips into the pixels of the lower mips

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = pSRV->GetDesc12();

    D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
    RTVDesc.Format = SRVDesc.Format;

    // Prep RTV desc with properties that are common across all mips
    // Not using arrayed RTVs because that would require a geometry shader
    switch(SRVDesc.ViewDimension)
    {
        case D3D12_SRV_DIMENSION_TEXTURE1D:
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            RTVDesc.Texture1DArray.ArraySize = 1;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE2D:
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            RTVDesc.Texture2D.PlaneSlice = 0;  // Only non-planar surfaces support mipmaps.
            break;
        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        case D3D12_SRV_DIMENSION_TEXTURECUBE:
        case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            RTVDesc.Texture2DArray.ArraySize = 1;
            RTVDesc.Texture2DArray.PlaneSlice = 0;  // Only non-planar surfaces support mipmaps.
            break;
        case D3D12_SRV_DIMENSION_TEXTURE3D:
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            RTVDesc.Texture3D.WSize = 1;
            break;
        default:
            ASSUME(false);
    }

    // Retrieve the appropriate PSO
    // The shaders used for the operation vary depending on resource dimension,
    // and the render target format must be baked into the PSO as well
    D3D12_RESOURCE_DIMENSION Dimension = pResource->Parent()->ResourceDimension12();
    DXGI_FORMAT Format = pSRV->GetDesc12().Format;
    ID3D12PipelineState* pGenMipsPSO = PrepareGenerateMipsObjects(Format, Dimension); // throw( _com_error, bad_alloc )

    UINT ResMipLevels = pResource->AppDesc()->MipLevels();

    // If there's no room for the SRV descriptor in the online heap, re-create it before applying state to the command list
    UINT ViewHeapSlot = ReserveSlots(m_ViewHeap, 1); // throw( _com_error )
    D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptor = m_ViewHeap.GPUHandle(ViewHeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptor = m_ViewHeap.CPUHandle(ViewHeapSlot);

    UINT SamplerHeapSlot = ReserveSlots(m_SamplerHeap, 1);
    D3D12_GPU_DESCRIPTOR_HANDLE GPUSamplerDescriptor = m_SamplerHeap.GPUHandle(SamplerHeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE CPUSamplerDescriptor = m_SamplerHeap.CPUHandle(SamplerHeapSlot);

#if DBG
    ID3D12GraphicsCommandList* pSnapshotCmdList = GetGraphicsCommandList();
#endif

    // All state that is applied for this operation must be overridden by the app's state on the next draw
    // Set the reassert bits to ensure that the next draw does the right state binding
    GetGraphicsCommandList()->SetPipelineState(pGenMipsPSO);
    m_StatesToReassert |= e_PipelineStateDirty;

    GetGraphicsCommandList()->SetGraphicsRootSignature(m_GenerateMipsRootSig.GetRootSignature());
    m_StatesToReassert |= e_GraphicsRootSignatureDirty;

    GetGraphicsCommandList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_StatesToReassert |= e_PrimitiveTopologyDirty;

    {
        // Unbind all VBs
        D3D12_VERTEX_BUFFER_VIEW VBVArray[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        memset(VBVArray, 0, sizeof(VBVArray));
        GetGraphicsCommandList()->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, VBVArray);

        m_StatesToReassert |= e_VertexBuffersDirty;
    }
        
    // Bind SRV
    if (SRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE || SRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY)
    {
        UINT NumCubes = 1; UINT MipLevels = 0;
        UINT FirstSlice = 0; UINT FirstMip = 0;
        if (SRVDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
        {
            MipLevels = SRVDesc.TextureCube.MipLevels;
            FirstMip = SRVDesc.TextureCube.MostDetailedMip;
        }
        else
        {
            MipLevels = SRVDesc.TextureCubeArray.MipLevels;
            FirstMip = SRVDesc.TextureCubeArray.MostDetailedMip;
            FirstSlice = SRVDesc.TextureCubeArray.First2DArrayFace;
            NumCubes = SRVDesc.TextureCubeArray.NumCubes;
        }
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        SRVDesc.Texture2DArray.MostDetailedMip = FirstMip;
        SRVDesc.Texture2DArray.MipLevels = MipLevels;
        SRVDesc.Texture2DArray.FirstArraySlice = FirstSlice;
        SRVDesc.Texture2DArray.ArraySize = NumCubes * 6;
        SRVDesc.Texture2DArray.PlaneSlice = 0;
        SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        m_pDevice12->CreateShaderResourceView( pResource->GetUnderlyingResource(), &SRVDesc, CPUDescriptor );
    }
    else
    {
        m_pDevice12->CopyDescriptorsSimple( 1, CPUDescriptor, pSRV->GetRefreshedDescriptorHandle(), m_ViewHeap.m_Desc.Type );
    }

    GetGraphicsCommandList()->SetGraphicsRootDescriptorTable(GenerateMipsRootSignatureSlots::eSRV, GPUDescriptor);

    // Bind Sampler
    m_pDevice12->CopyDescriptorsSimple(1, CPUSamplerDescriptor, m_GenerateMipsSamplers[FilterType], m_SamplerHeap.m_Desc.Type);
    GetGraphicsCommandList()->SetGraphicsRootDescriptorTable(GenerateMipsRootSignatureSlots::eSampler, GPUSamplerDescriptor);

    // Get RTV descriptor
    UINT RTVDescriptorHeapIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE RTVDescriptor = m_RTVAllocator.AllocateHeapSlot(&RTVDescriptorHeapIndex); // throw( _com_error )

    // For each contiguous range of subresources contained in the view
    for(auto subresourceRange : pSRV->m_subresources)
    {
        UINT start = subresourceRange.first;
        UINT end = subresourceRange.second;

        UINT minMip = start % ResMipLevels;
        UINT minSlice = start / ResMipLevels;
        for (UINT subresource = start + 1; subresource < end; ++subresource)
        {
            UINT mipLevel = subresource % ResMipLevels;
            if (mipLevel == 0)
            {
                // The view contains all the mips of some slices of an arrayed resource, so the outer loop includes multiple mip 0s
                assert(start % ResMipLevels == 0 && end % ResMipLevels == 0);
                continue;
            }
            UINT arraySlice = subresource / ResMipLevels;

            m_ResourceStateManager.TransitionSubresource(pResource, subresource - 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_ResourceStateManager.TransitionSubresource(pResource, subresource, D3D12_RESOURCE_STATE_RENDER_TARGET);
            m_ResourceStateManager.ApplyAllResourceTransitions();

            auto& Placement = pResource->GetSubresourcePlacement(subresource);
            D3D12_VIEWPORT Viewport = { 0, 0, (FLOAT)Placement.Footprint.Width, (FLOAT)Placement.Footprint.Height, 0, 1 };
            D3D12_RECT Scissor = { 0, 0, (LONG)Placement.Footprint.Width, (LONG)Placement.Footprint.Height };
            GetGraphicsCommandList()->RSSetViewports(1, &Viewport);
            GetGraphicsCommandList()->RSSetScissorRects(1, &Scissor);

            bool b3D = (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D);
            UINT numIterations = b3D ? Placement.Footprint.Depth : 1;
            for (UINT wSlice = 0; wSlice < numIterations; ++wSlice)
            {
                UINT arrayOrWSlice = b3D ? wSlice * 2 : arraySlice;
                switch(RTVDesc.ViewDimension)
                {
                    case D3D12_RTV_DIMENSION_TEXTURE1D:
                        RTVDesc.Texture1D.MipSlice = mipLevel;
                        break;
                    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
                        RTVDesc.Texture1DArray.MipSlice = mipLevel;
                        RTVDesc.Texture1DArray.FirstArraySlice = arraySlice;
                        break;
                    case D3D12_RTV_DIMENSION_TEXTURE2D:
                        RTVDesc.Texture2D.MipSlice = mipLevel;
                        break;
                    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
                        RTVDesc.Texture2DArray.MipSlice = mipLevel;
                        RTVDesc.Texture2DArray.FirstArraySlice = arraySlice;
                        break;
                    case D3D12_RTV_DIMENSION_TEXTURE3D:
                        assert(minSlice == 0);
                        RTVDesc.Texture3D.MipSlice = mipLevel;
                        RTVDesc.Texture3D.FirstWSlice = wSlice;
                        break;
                    default:
                        ASSUME(false);
                }

                m_pDevice12->CreateRenderTargetView(pResource->GetUnderlyingResource(), &RTVDesc, RTVDescriptor);
                GetGraphicsCommandList()->OMSetRenderTargets(1, &RTVDescriptor, false, nullptr);

                GetGraphicsCommandList()->SetGraphicsRoot32BitConstant(GenerateMipsRootSignatureSlots::eRootConstants, mipLevel - 1 - minMip, 0);
                GetGraphicsCommandList()->SetGraphicsRoot32BitConstant(GenerateMipsRootSignatureSlots::eRootConstants, arrayOrWSlice - minSlice, 1);

                if (b3D)
                {
                    // We calculate the w value here for 3D textures to avoid requiring the shader to calculate it
                    // every pixel. Add a half point to the wSlice to make sure we sample from the two relevant
                    // z planes equally
                    float wVal = (wSlice + .5f) / numIterations;
                    GetGraphicsCommandList()->SetGraphicsRoot32BitConstant(GenerateMipsRootSignatureSlots::eRootConstants, *(UINT*) (&wVal), 2);
                }

                GetGraphicsCommandList()->DrawInstanced(4, 1, 0, 0);
            }
        }
    }

    // Free RTV descriptor
    m_RTVAllocator.FreeHeapSlot(RTVDescriptor, RTVDescriptorHeapIndex);

    // Dirty bits for multiply-updated states
    m_StatesToReassert |= e_RenderTargetsDirty;
    m_StatesToReassert |= e_ViewportsDirty;
    m_StatesToReassert |= e_ScissorRectsDirty;

    // Ensure that there were no flushes during the GenMips process
    // Since the state was applied directly to the command list, it will NOT be re-applied to a new command list
#if DBG // Required for OACR
    assert(GetGraphicsCommandList() == pSnapshotCmdList);
#endif

    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::UAVBarrier() noexcept
{
    D3D12_RESOURCE_BARRIER BarrierDesc;
    ZeroMemory(&BarrierDesc, sizeof(BarrierDesc));
    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

    GetGraphicsCommandList()->ResourceBarrier(1, &BarrierDesc);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CopyAndConvertSubresourceRegion(Resource* pDst, UINT DstSubresource, Resource* pSrc, UINT SrcSubresource, UINT dstX, UINT dstY, UINT dstZ, const D3D12_BOX* pSrcBox) noexcept
{
    assert(!Resource::IsSameUnderlyingSubresource(pSrc, SrcSubresource, pDst, DstSubresource));

    struct CopyDesc
    {
        Resource* pResource;
        D3D12_TEXTURE_COPY_LOCATION View;
    } Descs[2];


    Descs[0].pResource = pSrc;
    Descs[0].View.SubresourceIndex = SrcSubresource;
    Descs[0].View.pResource = pSrc->GetUnderlyingResource();
    Descs[1].pResource = pDst;
    Descs[1].View.SubresourceIndex = DstSubresource;
    Descs[1].View.pResource = pDst->GetUnderlyingResource();

    DXGI_FORMAT DefaultResourceFormat = DXGI_FORMAT_UNKNOWN;

    D3D12_BOX PatchedBox = {};
    if (!pSrcBox)
    {
        PatchedBox = GetBoxFromResource(pSrc, SrcSubresource);
        pSrcBox = &PatchedBox;
    }

    for (UINT i = 0; i < 2; ++i)
    {
        if (Descs[i].pResource->m_Identity->m_bOwnsUnderlyingResource && !Descs[i].pResource->m_Identity->m_bPlacedTexture)
        {
            Descs[i].View.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            DefaultResourceFormat = Descs[i].pResource->GetSubresourcePlacement(Descs[i].View.SubresourceIndex).Footprint.Format;
        }
        else
        {
            Descs[i].View.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            Descs[i].View.PlacedFootprint = Descs[i].pResource->GetSubresourcePlacement(Descs[i].View.SubresourceIndex);
        }
    }

    bool bConvertSrc = false, bConvertDst = false;

    if (pSrc->AppDesc()->NonOpaquePlaneCount() == 1 && pDst->AppDesc()->NonOpaquePlaneCount() == 1)
    {
        if (DefaultResourceFormat == DXGI_FORMAT_UNKNOWN)
        {
            // No default resources, or two buffers, check if we have to convert one
            DefaultResourceFormat = Descs[1].pResource->GetSubresourcePlacement(DstSubresource).Footprint.Format;
            bConvertDst = Descs[0].pResource->GetSubresourcePlacement(SrcSubresource).Footprint.Format != DefaultResourceFormat;
            if (!bConvertDst && DefaultResourceFormat == DXGI_FORMAT_UNKNOWN)
            {
                // This is a buffer to buffer copy
                // Special-case buffer to buffer copies
                assert(Descs[0].pResource->AppDesc()->ResourceDimension() == D3D11_RESOURCE_DIMENSION_BUFFER &&
                       Descs[1].pResource->AppDesc()->ResourceDimension() == D3D11_RESOURCE_DIMENSION_BUFFER);

                UINT64 SrcOffset = pSrcBox->left + GetDynamicBufferOffset(pSrc);
                UINT64 Size = pSrcBox->right - pSrcBox->left;
                GetGraphicsCommandList()->CopyBufferRegion(pDst->GetUnderlyingResource(), dstX + GetDynamicBufferOffset(pDst),
                                                 pSrc->GetUnderlyingResource(), SrcOffset, Size);
                return;
            }
        }
        else if (Descs[0].View.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX &&
                 Descs[1].View.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
        {
            // No conversion
        }
        else if (Descs[0].View.Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT &&
                 Descs[0].pResource->GetSubresourcePlacement(SrcSubresource).Footprint.Format != DefaultResourceFormat)
        {
            assert(Descs[1].View.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX);
            bConvertSrc = true;
        }
        else if (Descs[1].pResource->GetSubresourcePlacement(DstSubresource).Footprint.Format != DefaultResourceFormat)
        {
            assert(Descs[0].View.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX);
            assert(Descs[1].View.Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT);
            bConvertDst = true;
        }
    }

    // Convert placement struct
    if (bConvertSrc || bConvertDst)
    {
        assert(pSrc->AppDesc()->NonOpaquePlaneCount() == 1 && pDst->AppDesc()->NonOpaquePlaneCount() == 1);
        
        UINT ConversionIndex = bConvertDst ? 1 : 0;
        UINT WidthAlignment[2], HeightAlignment[2];
        auto& Placement = Descs[ConversionIndex].View.PlacedFootprint.Footprint;
        DXGI_FORMAT& Format = Placement.Format;

        WidthAlignment[0] = CD3D11FormatHelper::GetWidthAlignment( Format );
        HeightAlignment[0] = CD3D11FormatHelper::GetHeightAlignment( Format );
        Format = DefaultResourceFormat;
        WidthAlignment[1] = CD3D11FormatHelper::GetWidthAlignment( Format );
        HeightAlignment[1] = CD3D11FormatHelper::GetHeightAlignment( Format );

        Placement.Width  = Placement.Width  * WidthAlignment[1] / WidthAlignment[0];
        Placement.Height = Placement.Height * HeightAlignment[1] / HeightAlignment[0];

        // Convert coordinates/box
        if (bConvertSrc)
        {
            assert(pSrcBox);
            if (pSrcBox != &PatchedBox)
            {
                PatchedBox = *pSrcBox;
                pSrcBox = &PatchedBox;
            }

            PatchedBox.left   = PatchedBox.left   * WidthAlignment[1] / WidthAlignment[0];
            PatchedBox.right  = PatchedBox.right  * WidthAlignment[1] / WidthAlignment[0];
            PatchedBox.top    = PatchedBox.top    * HeightAlignment[1] / HeightAlignment[0];
            PatchedBox.bottom = PatchedBox.bottom * HeightAlignment[1] / HeightAlignment[0];
        }
        else if (bConvertDst)
        {
            dstX = dstX * WidthAlignment[1] / WidthAlignment[0];
            dstY = dstY * HeightAlignment[1] / HeightAlignment[0];
        }
    }
    
    // Actually issue the copy!
    GetGraphicsCommandList()->CopyTextureRegion(&Descs[1].View, dstX, dstY, dstZ, &Descs[0].View, pSrcBox);
}


//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ResourceCopy(Resource* pDst, Resource* pSrc )
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    assert(pSrc->NumSubresources() == pDst->NumSubresources());
    if (Resource::IsSameUnderlyingSubresource(pSrc, 0, pDst, 0))
    {
#ifdef USE_PIX
        PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Whole-resource copy");
#endif
        for (UINT Subresource = 0; Subresource < pSrc->NumSubresources(); ++Subresource)
        {
            SameResourceCopy(pDst, Subresource, pSrc, Subresource, 0, 0, 0, nullptr);
        }
    }
    else
    {
        m_ResourceStateManager.TransitionResource(pDst, D3D12_RESOURCE_STATE_COPY_DEST);
        m_ResourceStateManager.TransitionResource(pSrc, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_ResourceStateManager.ApplyAllResourceTransitions();
        
        // Note that row-major placed textures must not be passed to CopyResource, because their underlying
        // buffer's size does not include padding for alignment to pool sizes like for staging textures.
        if ((pSrc->m_Identity->m_bOwnsUnderlyingResource && pDst->m_Identity->m_bOwnsUnderlyingResource &&
            !pSrc->m_Identity->m_bPlacedTexture && !pDst->m_Identity->m_bPlacedTexture &&
            pSrc->GetOffsetToStreamOutputSuffix() == pDst->GetOffsetToStreamOutputSuffix() &&
            pSrc->IsBloatedConstantBuffer() == pDst->IsBloatedConstantBuffer()))
        {
            // Neither resource should be suballocated, so no offset adjustment required
            // We can do a straight resource copy from heap to heap
#if DBG
            auto& DstPlacement = pDst->GetSubresourcePlacement(0);
            auto& SrcPlacement = pSrc->GetSubresourcePlacement(0);
            assert(SrcPlacement.Offset == 0 && DstPlacement.Offset == 0 && SrcPlacement.Footprint.RowPitch == DstPlacement.Footprint.RowPitch);
#endif

            auto pAPIDst = pDst->GetUnderlyingResource();
            auto pAPISrc = pSrc->GetUnderlyingResource();
            GetGraphicsCommandList()->CopyResource(pAPIDst, pAPISrc);
        }
        else
        {
#ifdef USE_PIX
            PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Whole-resource copy");
#endif
            for (UINT Subresource = 0; Subresource < pSrc->NumSubresources(); ++Subresource)
            {
                CopyAndConvertSubresourceRegion(pDst, Subresource, pSrc, Subresource, 0, 0, 0, nullptr);
            }
        }
    }
    PostCopy(pSrc, 0, pDst, 0, pSrc->NumSubresources());
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ResourceResolveSubresource(Resource* pDst, UINT DstSubresource, Resource* pSrc, UINT SrcSubresource, DXGI_FORMAT Format )
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    assert(pDst->m_Identity->m_bOwnsUnderlyingResource);
    assert(pSrc->m_Identity->m_bOwnsUnderlyingResource);
    
    assert(pSrc->SubresourceMultiplier() == pDst->SubresourceMultiplier());

    // Originally this would loop based on SubResourceMultiplier, allowing multiple planes to be resolved.
    // In practice, this was really only hit by depth+stencil formats, but we can only resolve the depth portion
    // since resolving the S8_UINT bit using averaging isn't valid.
    // Input subresources are plane-extended, except when dealing with depth+stencil
    UINT CurSrcSub = pSrc->GetExtendedSubresourceIndex(SrcSubresource, 0);
    UINT CurDstSub = pDst->GetExtendedSubresourceIndex(DstSubresource, 0);
    m_ResourceStateManager.TransitionSubresource(pDst, CurDstSub, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    m_ResourceStateManager.TransitionSubresource(pSrc, CurSrcSub, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto pAPIDst = pDst->GetUnderlyingResource();
    auto pAPISrc = pSrc->GetUnderlyingResource();

    // ref for formats supporting MSAA resolve https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/format-support-for-direct3d-11-1-feature-level-hardware

    switch(Format)
    {
        // Can't resolve due to stencil UINT. Claim it's typeless and just resolve the depth plane
        case DXGI_FORMAT_D24_UNORM_S8_UINT: Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; break;
        // Can't resolve this particular flavor of depth format. Claim it's R16_UNORM instead
        case DXGI_FORMAT_D16_UNORM: Format = DXGI_FORMAT_R16_UNORM; break;
    }
    GetGraphicsCommandList()->ResolveSubresource(pAPIDst, CurDstSub, pAPISrc, CurSrcSub, Format);
    PostRender(COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetResourceMinLOD(Resource* pResource, FLOAT MinLOD )
{
    pResource->SetMinLOD(MinLOD);
    ++(pResource->m_SRVUniqueness);
    
    // Mark all potential SRV bind points as dirty
    CResourceBindings& bindingState = pResource->m_currentBindings;
    for (auto pCur = bindingState.m_ShaderResourceViewList.Flink;
         pCur != &bindingState.m_ShaderResourceViewList;
         pCur = pCur->Flink)
    {
        auto& viewBindings = *CONTAINING_RECORD(pCur, CViewBindings<ShaderResourceViewType>, m_ViewBindingList);
        for (UINT stage = 0; stage < ShaderStageCount; ++stage)
        {
            auto& stageState = m_CurrentState.GetStageState((EShaderStage)stage);
            stageState.m_SRVs.SetDirtyBits(viewBindings.m_BindPoints[stage]);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CopyStructureCount(Resource* pDstResource, UINT DstAlignedByteOffset, UAV *pUAV)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"CopyStructureCount");
#endif
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    pUAV->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

    m_ResourceStateManager.TransitionResource(pDstResource, D3D12_RESOURCE_STATE_COPY_DEST);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto pAPIDst = pDstResource->GetUnderlyingResource();
    
    UINT BufferOffset = DstAlignedByteOffset + GetDynamicBufferOffset(pDstResource);

    pUAV->CopyCounterToBuffer(pAPIDst, BufferOffset);

    PostCopy(nullptr, 0, pDstResource, 0, pDstResource->NumSubresources());
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ResourceCopyRegion(Resource* pDst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, Resource* pSrc, UINT SrcSubresource, const D3D12_BOX* pSrcBox)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    assert(pSrc->SubresourceMultiplier() == pDst->SubresourceMultiplier());
    UINT SubresourceMultiplier = pSrc->SubresourceMultiplier();
    for (UINT i = 0; i < SubresourceMultiplier; ++i)
    {
        // Input subresources are plane-extended, except when dealing with depth+stencil
        UINT CurSrcSub = pSrc->GetExtendedSubresourceIndex(SrcSubresource, i);
        UINT CurDstSub = pDst->GetExtendedSubresourceIndex(DstSubresource, i);

        if (Resource::IsSameUnderlyingSubresource(pSrc, CurSrcSub, pDst, CurDstSub))
        {
            SameResourceCopy(pDst, CurDstSub, pSrc, CurSrcSub, DstX, DstY, DstZ, pSrcBox);
        }
        else
        {
            m_ResourceStateManager.TransitionSubresource(pDst, CurDstSub, D3D12_RESOURCE_STATE_COPY_DEST);
            m_ResourceStateManager.TransitionSubresource(pSrc, CurSrcSub, D3D12_RESOURCE_STATE_COPY_SOURCE);
            m_ResourceStateManager.ApplyAllResourceTransitions();

            CopyAndConvertSubresourceRegion(pDst, CurDstSub, pSrc, CurSrcSub, DstX, DstY, DstZ, pSrcBox);
        }

        PostCopy(pSrc, CurSrcSub, pDst, CurDstSub, 1);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::UploadDataToMappedBuffer(_In_reads_bytes_(Placement.Depth * DepthPitch) const void* pData, UINT SrcPitch, UINT SrcDepth,
                                       _Out_writes_bytes_(Placement.Depth * DepthPitch) void* pMappedData,
                                       D3D12_SUBRESOURCE_FOOTPRINT& Placement, UINT DepthPitch, UINT TightRowPitch) noexcept
{
    bool bPlanar = !!CD3D11FormatHelper::Planar(Placement.Format);
    UINT NumRows = bPlanar ? Placement.Height : Placement.Height / CD3D11FormatHelper::GetHeightAlignment(Placement.Format);

    ASSUME(TightRowPitch <= DepthPitch);
    ASSUME(TightRowPitch <= Placement.RowPitch);
    ASSUME(NumRows <= Placement.Height);
    ASSUME(Placement.RowPitch * NumRows <= DepthPitch);

    // Fast-path: app gave us aligned memory
    if ((Placement.RowPitch == SrcPitch || Placement.Height == 1) &&
        (DepthPitch == SrcDepth || Placement.Depth == 1))
    {
        // Allow last row to be non-padded
        UINT CopySize = DepthPitch * (Placement.Depth - 1) +
            Placement.RowPitch * (NumRows - 1) +
            TightRowPitch;
        memcpy(pMappedData, pData, CopySize);
    }
    else
    {
        // Slow path: row-by-row memcpy
        D3D12_MEMCPY_DEST Dest = { pMappedData, Placement.RowPitch, DepthPitch };
        D3D12_SUBRESOURCE_DATA Src = { pData, (LONG_PTR)SrcPitch, (LONG_PTR)SrcDepth };
        MemcpySubresource(&Dest, &Src, TightRowPitch, NumRows, Placement.Depth);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template<typename TInterleaved, typename TPlanar, TInterleaved Mask, TPlanar Shift>
void DeInterleaving2DCopy(
    _In_reads_(_Inexpressible_(sizeof(TInterleaved) * Width + SrcRowPitch * (Height - 1))) const BYTE* pSrcData,
    UINT SrcRowPitch,
    _Out_writes_(_Inexpressible_(sizeof(TPlanar) * Width + DstRowPitch * (Height - 1))) BYTE* pDstData,
    UINT DstRowPitch, UINT Width, UINT Height)
{
    static_assert(sizeof(TInterleaved) >= sizeof(TPlanar), "Invalid types used for interleaving copy.");

    for (UINT y = 0; y < Height; ++y)
    {
        const TInterleaved* pSrcRow = reinterpret_cast<const TInterleaved*>(pSrcData + SrcRowPitch * y);
        TPlanar* pDstRow = reinterpret_cast<TPlanar*>(pDstData + DstRowPitch * y);
        for (UINT x = 0; x < Width; ++x)
        {
            pDstRow[x] = static_cast<TPlanar>((pSrcRow[x] & Mask) >> Shift);
        }
    }
}

template<typename TInterleaved, typename TPlanar, TPlanar Mask, TPlanar Shift>
void Interleaving2DCopy(
    _In_reads_(_Inexpressible_(sizeof(TPlanar) * Width + SrcRowPitch * (Height - 1))) const BYTE* pSrcData,
    UINT SrcRowPitch,
    _Out_writes_(_Inexpressible_(sizeof(TInterleaved) * Width + DstRowPitch * (Height - 1))) BYTE* pDstData,
    UINT DstRowPitch, UINT Width, UINT Height)
{
    static_assert(sizeof(TInterleaved) >= sizeof(TPlanar), "Invalid types used for interleaving copy.");

    for (UINT y = 0; y < Height; ++y)
    {
        const TPlanar* pSrcRow = reinterpret_cast<const TPlanar*>(pSrcData + SrcRowPitch * y);
        TInterleaved* pDstRow = reinterpret_cast<TInterleaved*>(pDstData + DstRowPitch * y);
        for (UINT x = 0; x < Width; ++x)
        {
            pDstRow[x] |= (static_cast<TInterleaved>(pSrcRow[x] & Mask) << Shift);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void DepthStencilDeInterleavingUpload(DXGI_FORMAT ParentFormat, UINT PlaneIndex, const BYTE* pSrcData, UINT SrcRowPitch, BYTE* pDstData, UINT DstRowPitch, UINT Width, UINT Height)
{
    ASSUME(PlaneIndex == 0 || PlaneIndex == 1);
    switch (ParentFormat)
    {
        case DXGI_FORMAT_R24G8_TYPELESS:
        {
            if (PlaneIndex == 0)
                DeInterleaving2DCopy<UINT, UINT, 0x00ffffff, 0>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
            else
                DeInterleaving2DCopy<UINT, UINT8, 0xff000000, 24>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
        } break;
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        {
            if (PlaneIndex == 0)
                DeInterleaving2DCopy<UINT64, UINT, 0x00000000ffffffff, 0>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
            else
                DeInterleaving2DCopy<UINT64, UINT8, 0x000000ff00000000, 32>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
        } break;
        default: ASSUME(false);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void DepthStencilInterleavingReadback(DXGI_FORMAT ParentFormat, UINT PlaneIndex, const BYTE* pSrcData, UINT SrcRowPitch, BYTE* pDstData, UINT DstRowPitch, UINT Width, UINT Height)
{
    ASSUME(PlaneIndex == 0 || PlaneIndex == 1);
    switch (ParentFormat)
    {
        case DXGI_FORMAT_R24G8_TYPELESS:
        {
            if (PlaneIndex == 0)
                Interleaving2DCopy<UINT, UINT, 0x00ffffff, 0>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
            else
                Interleaving2DCopy<UINT, UINT8, 0xff, 24>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
        } break;
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        {
            if (PlaneIndex == 0)
                Interleaving2DCopy<UINT64, UINT, 0xffffffff, 0>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
            else
                Interleaving2DCopy<UINT64, UINT8, 0xff, 32>(pSrcData, SrcRowPitch, pDstData, DstRowPitch, Width, Height);
        } break;
        default: ASSUME(false);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT Swap10bitRBPixel(UINT pixel)
{
    constexpr UINT alphaMask = 3u << 30u;
    constexpr UINT blueMask = 0x3FFu << 20u;
    constexpr UINT greenMask = 0x3FFu << 10u;
    constexpr UINT redMask = 0x3FFu;
    return (pixel & (alphaMask | greenMask)) |
        ((pixel & blueMask) >> 20u) |
        ((pixel & redMask) << 20u);
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void Swap10bitRBUpload(const BYTE* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch,
                              BYTE* pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                              UINT Width, UINT Height, UINT Depth)
{
    for (UINT z = 0; z < Depth; ++z)
    {
        auto pSrcSlice = pSrcData + SrcDepthPitch * z;
        auto pDstSlice = pDstData + DstDepthPitch * z;
        for (UINT y = 0; y < Height; ++y)
        {
            auto pSrcRow = pSrcSlice + SrcRowPitch * y;
            auto pDstRow = pDstSlice + DstRowPitch * y;
            for (UINT x = 0; x < Width; ++x)
            {
                reinterpret_cast<UINT*>(pDstRow)[x] = Swap10bitRBPixel(reinterpret_cast<const UINT*>(pSrcRow)[x]);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
_Use_decl_annotations_
void ImmediateContext::FinalizeUpdateSubresources(Resource* pDst, PreparedUpdateSubresourcesOperation const& PreparedStorage, D3D12_PLACED_SUBRESOURCE_FOOTPRINT const* LocalPlacementDescs)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"UpdateSubresource on GPU timeline");
#endif
    bool bUseLocalPlacement = LocalPlacementDescs != nullptr;

    const UINT8 PlaneCount = (pDst->SubresourceMultiplier() * pDst->AppDesc()->NonOpaquePlaneCount());

    D3D12ResourceSuballocation mappableResource = PreparedStorage.EncodedBlock.Decode();
    CViewSubresourceSubset SubresourceIteration(PreparedStorage.EncodedSubresourceSubset, pDst->AppDesc()->MipLevels(), pDst->AppDesc()->ArraySize(), PlaneCount);

    // Copy contents over from the temporary upload heap 
    ID3D12GraphicsCommandList *pGraphicsCommandList = GetGraphicsCommandList();

    m_ResourceStateManager.TransitionSubresources(pDst, SubresourceIteration, D3D12_RESOURCE_STATE_COPY_DEST);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto DoFinalize = [&]()
    {
        UINT PlacementIdx = 0;
        for (const auto& it : SubresourceIteration)
        {
            for (UINT Subresource = it.first; Subresource < it.second; ++Subresource, ++PlacementIdx)
            {
                auto& Placement = bUseLocalPlacement ? LocalPlacementDescs[PlacementIdx] : pDst->GetSubresourcePlacement(Subresource);
                D3D12_BOX srcBox = { 0, 0, 0, Placement.Footprint.Width, Placement.Footprint.Height, Placement.Footprint.Depth };
                if (pDst->AppDesc()->ResourceDimension() == D3D12_RESOURCE_DIMENSION_BUFFER)
                {
                    ASSUME(Placement.Footprint.Height == 1 && Placement.Footprint.Depth == 1);
                    UINT64 srcOffset = mappableResource.GetOffset();
                    UINT64 dstOffset =
                        pDst->GetSubresourcePlacement(0).Offset +
                        (PreparedStorage.bDstBoxPresent ? PreparedStorage.DstX : 0);
                    pGraphicsCommandList->CopyBufferRegion(pDst->GetUnderlyingResource(),
                                                           dstOffset,
                                                           mappableResource.GetResource(),
                                                           srcOffset,
                                                           Placement.Footprint.Width);
                }
                else
                {
                    D3D12_TEXTURE_COPY_LOCATION SrcDesc = mappableResource.GetCopyLocation(Placement);
                    SrcDesc.PlacedFootprint.Offset -= PreparedStorage.OffsetAdjustment;
                    bool bDstPlacedTexture = !pDst->m_Identity->m_bOwnsUnderlyingResource || pDst->m_Identity->m_bPlacedTexture;
                    D3D12_TEXTURE_COPY_LOCATION DstDesc = bDstPlacedTexture ?
                        CD3DX12_TEXTURE_COPY_LOCATION(pDst->GetUnderlyingResource(), pDst->GetSubresourcePlacement(Subresource)) :
                        CD3DX12_TEXTURE_COPY_LOCATION(pDst->GetUnderlyingResource(), Subresource);
                    pGraphicsCommandList->CopyTextureRegion(&DstDesc,
                                                            PreparedStorage.DstX,
                                                            PreparedStorage.DstY,
                                                            PreparedStorage.DstZ,
                                                            &SrcDesc,
                                                            &srcBox);
                }
            }
        }
    };

    if (PreparedStorage.bDisablePredication)
    {
        CDisablePredication DisablePredication(this);
        DoFinalize();
    }
    else
    {
        DoFinalize();
    }

    AdditionalCommandsAdded(COMMAND_LIST_TYPE::GRAPHICS);
    PostUpload();

    ReleaseSuballocatedHeap(AllocatorHeapType::Upload, mappableResource, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS), COMMAND_LIST_TYPE::GRAPHICS);
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::CPrepareUpdateSubresourcesHelper::CPrepareUpdateSubresourcesHelper(
    Resource& Dst,
    CSubresourceSubset const& Subresources,
    const D3D11_SUBRESOURCE_DATA* pSrcData,
    const D3D12_BOX* pDstBox,
    UpdateSubresourcesFlags flags,
    const void* pClearPattern,
    UINT ClearPatternSize,
    ImmediateContext& ImmCtx)
    : Dst(Dst)
    , Subresources(Subresources)
    , bDstBoxPresent(pDstBox != nullptr)
{
#ifdef USE_PIX
    PIXScopedEvent(0ull, L"UpdateSubresource on CPU timeline");
#endif
#if DBG
    AssertPreconditions(pSrcData, pClearPattern);
#endif
    bool bEmptyBox = InitializePlacementsAndCalculateSize(pDstBox, ImmCtx.m_pDevice12.get());
    if (bEmptyBox)
    {
        return;
    }

    InitializeMappableResource(flags, ImmCtx, pDstBox);
    FinalizeNeeded = CachedNeedsTemporaryUploadHeap;

    UploadDataToMappableResource(pSrcData, ImmCtx, pDstBox, pClearPattern, ClearPatternSize, flags);

    if (FinalizeNeeded)
    {
        WriteOutputParameters(pDstBox, flags);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
#if DBG
void ImmediateContext::CPrepareUpdateSubresourcesHelper::AssertPreconditions(const D3D11_SUBRESOURCE_DATA* pSrcData, const void* pClearPattern)
{
    // Currently only handles initial data and UpdateSubresource-type operations
    // This means: 1 plane, 1 legacy subresource, or all subresources with no box
    assert(NumSrcData == 1U || (NumSrcData == static_cast<UINT>(Dst.AppDesc()->MipLevels() * Dst.AppDesc()->ArraySize()) && !bDstBoxPresent && !pClearPattern));
    assert(NumDstSubresources == 1U || NumDstSubresources == Dst.SubresourceMultiplier() || (NumDstSubresources == Dst.NumSubresources() && !bDstBoxPresent && !pClearPattern));

    // This routine accepts either a clear color (one pixel worth of data) or a SUBRESOURCE_DATA struct (minimum one row of data)
    assert(!(pClearPattern && pSrcData));
    ASSUME(!bUseLocalPlacement || NumDstSubresources == 1 || (NumDstSubresources == 2 && Subresources.m_EndPlane - Subresources.m_BeginPlane == 2));

    CViewSubresourceSubset SubresourceIteration(Subresources, Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize(), PlaneCount);
    assert(!SubresourceIteration.IsEmpty());
}
#endif

//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::CPrepareUpdateSubresourcesHelper::InitializePlacementsAndCalculateSize(const D3D12_BOX* pDstBox, ID3D12Device* pDevice)
{
    auto& LocalPlacementDescs = PreparedStorage.LocalPlacementDescs;

    // How big of an intermediate do we need?
    // If we need to use local placement structs, fill those out as well
    if (bUseLocalPlacement)
    {
        for (UINT i = 0; i < NumDstSubresources; ++i)
        {
            auto& PlacementDesc = LocalPlacementDescs[i];
            UINT Subresource = ComposeSubresourceIdxExtended(Subresources.m_BeginMip, Subresources.m_BeginArray, Subresources.m_BeginPlane + i, Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize());
            UINT SlicePitch;
            if (pDstBox)
            {
                // No-op
                if (pDstBox->right <= pDstBox->left ||
                    pDstBox->bottom <= pDstBox->top ||
                    pDstBox->back <= pDstBox->front)
                {
                    return true;
                }

                // Note: D3D11 provides a subsampled box, so for planar formats, we need to use the plane format to avoid subsampling again
                Resource::FillSubresourceDesc(pDevice,
                                                Dst.GetSubresourcePlacement(Subresource).Footprint.Format,
                                                pDstBox->right - pDstBox->left,
                                                pDstBox->bottom - pDstBox->top,
                                                pDstBox->back - pDstBox->front,
                                                PlacementDesc);

                CD3D11FormatHelper::CalculateMinimumRowMajorSlicePitch(PlacementDesc.Footprint.Format,
                                                                        PlacementDesc.Footprint.RowPitch,
                                                                        PlacementDesc.Footprint.Height,
                                                                        SlicePitch);
            }
            else
            {
                PlacementDesc = Dst.GetSubresourcePlacement(Subresource);
                SlicePitch = Dst.DepthPitch(Subresource);
            }
            PlacementDesc.Offset = TotalSize;

            TotalSize += Align<UINT64>(static_cast<UINT64>(SlicePitch) * PlacementDesc.Footprint.Depth, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        }
    }
    else
    {
        auto& Placement = Dst.GetSubresourcePlacement(LastDstSubresource);

        // If the destination is suballocated, make sure to factor out the suballocated offset when calculating how
        // large the resource needs to be
        UINT64 suballocationOffset = Dst.m_Identity->m_bOwnsUnderlyingResource ? 0 : Dst.m_Identity->GetSuballocatedOffset();

        TotalSize =
            (Placement.Offset - suballocationOffset) +
            static_cast<UINT64>(Dst.DepthPitch(LastDstSubresource)) * Placement.Footprint.Depth -
            (Dst.GetSubresourcePlacement(FirstDstSubresource).Offset - suballocationOffset);
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Only respect predication in response to an actual UpdateSubresource (or similar) API call.
// Internal uses of UpdateSubresource, as well as initial data, should ignore predication.
// Batched update operations cannot even query predication and must assume a copy must be used.
bool ImmediateContext::CPrepareUpdateSubresourcesHelper::NeedToRespectPredication(UpdateSubresourcesFlags flags) const
{
    return (flags & UpdateSubresourcesFlags::ScenarioMask) == UpdateSubresourcesFlags::ScenarioImmediateContext;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::CPrepareUpdateSubresourcesHelper::NeedTemporaryUploadHeap(UpdateSubresourcesFlags flags , ImmediateContext& ImmCtx) const
{
    UpdateSubresourcesFlags scenario = (flags & UpdateSubresourcesFlags::ScenarioMask);
    bool bCanWriteDirectlyToResource =
        scenario != UpdateSubresourcesFlags::ScenarioBatchedContext &&      // If we aren't explicitly requesting a copy to a temp...
        (!NeedToRespectPredication(flags) || !ImmCtx.m_CurrentState.m_pPredicate) &&      // And we don't need to respect predication...
        !Dst.GetIdentity()->m_bOwnsUnderlyingResource &&             // And the resource came from a pool...
        Dst.GetAllocatorHeapType() != AllocatorHeapType::Readback;   // And it's not the readback pool...
    if (bCanWriteDirectlyToResource && scenario != UpdateSubresourcesFlags::ScenarioInitialData)
    {
        // Check if resource is idle.
        CViewSubresourceSubset SubresourceIteration(Subresources, Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize(), PlaneCount);
        for (auto&& range : SubresourceIteration)
        {
            for (UINT i = range.first; i < range.second; ++i)
            {
                if (!ImmCtx.SynchronizeForMap(&Dst, i, MAP_TYPE_WRITE, true))
                {
                    bCanWriteDirectlyToResource = false;
                    break;
                }
            }
            if (!bCanWriteDirectlyToResource)
            {
                break;
            }
        }
    }
    // ... And it's not busy, then we can do this upload operation directly into the final destination resource.
    return !bCanWriteDirectlyToResource;
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CPrepareUpdateSubresourcesHelper::InitializeMappableResource(UpdateSubresourcesFlags flags, ImmediateContext& ImmCtx, D3D12_BOX const* pDstBox)
{
    UpdateSubresourcesFlags scenario = flags & UpdateSubresourcesFlags::ScenarioMask;
    CachedNeedsTemporaryUploadHeap = NeedTemporaryUploadHeap(flags, ImmCtx);
    if (CachedNeedsTemporaryUploadHeap)
    {
        ResourceAllocationContext threadingContext = ResourceAllocationContext::ImmediateContextThreadTemporary;
        if ((scenario == UpdateSubresourcesFlags::ScenarioInitialData && ImmCtx.m_CreationArgs.CreatesAndDestroysAreMultithreaded) ||
            scenario == UpdateSubresourcesFlags::ScenarioBatchedContext)
        {
            threadingContext = ResourceAllocationContext::FreeThread;
        }
        mappableResource = ImmCtx.AcquireSuballocatedHeap(AllocatorHeapType::Upload, TotalSize, threadingContext); // throw( _com_error )
    }
    else
    {
        if (pDstBox)
        {
            // Only DX9 managed vertex buffers and dx11 padded constant buffers hit this path, so extra complexity isn't required yet
            assert(Dst.Parent()->m_desc12.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
            assert(!bDeInterleavingUpload && Dst.GetSubresourcePlacement(0).Footprint.Format == DXGI_FORMAT_UNKNOWN);
            assert(pDstBox->top == 0 && pDstBox->front == 0);

            bufferOffset = pDstBox ? pDstBox->left : 0;
        }

        mappableResource = Dst.GetIdentity()->m_suballocation;
    }
    assert(mappableResource.IsInitialized());
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CPrepareUpdateSubresourcesHelper::UploadSourceDataToMappableResource(void* pDstData, D3D11_SUBRESOURCE_DATA const* pSrcData, ImmediateContext& ImmCtx, UpdateSubresourcesFlags flags)
{
    // The source data array provided is indexed by D3D11.0 subresource indices
    for (UINT SrcDataIdx = 0; SrcDataIdx < NumSrcData; ++SrcDataIdx)
    {
        auto& SrcData = pSrcData[SrcDataIdx];
        UINT ArraySlice, MipLevel;
        DecomposeSubresourceIdxNonExtended(SrcDataIdx, Subresources.m_EndMip - Subresources.m_BeginMip, MipLevel, ArraySlice);

        const BYTE* pSrcPlaneData = reinterpret_cast<const BYTE*>(SrcData.pSysMem);

        // Even though the next subresource is supposed to be the next mip, planes are iterated last so that the pointer adjustment
        // for planar source data doesn't need to be calculated a second time
        for (UINT Plane = Subresources.m_BeginPlane; Plane < Subresources.m_EndPlane; ++Plane)
        {
            const UINT Subresource = ComposeSubresourceIdxExtended(MipLevel + Subresources.m_BeginMip, ArraySlice + Subresources.m_BeginArray, Plane,
                                                                    Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize());
            auto& Placement = bUseLocalPlacement ? PreparedStorage.LocalPlacementDescs[Plane - Subresources.m_BeginPlane] : Dst.GetSubresourcePlacement(Subresource);

            BYTE* pDstSubresourceData = reinterpret_cast<BYTE*>(pDstData) + Placement.Offset - PreparedStorage.Base.OffsetAdjustment;

            // If writing directly into the resource, we need to account for the dstBox instead of leaving it to the GPU copy
            if (!CachedNeedsTemporaryUploadHeap)
            {
                pDstSubresourceData += bufferOffset;
            }

            if (bDeInterleavingUpload)
            {
                DepthStencilDeInterleavingUpload(ImmCtx.GetParentForFormat(Dst.AppDesc()->Format()), Plane,
                                                    pSrcPlaneData, SrcData.SysMemPitch,
                                                    pDstSubresourceData, Placement.Footprint.RowPitch,
                                                    Placement.Footprint.Width, Placement.Footprint.Height);
                // Intentionally not advancing the src pointer, since the next copy reads from the same data
            }
            else if ((flags & UpdateSubresourcesFlags::ChannelSwapR10G10B10A2) != UpdateSubresourcesFlags::None)
            {
                Swap10bitRBUpload(pSrcPlaneData, SrcData.SysMemPitch, SrcData.SysMemSlicePitch,
                                  pDstSubresourceData, Placement.Footprint.RowPitch, Dst.DepthPitch(Subresource),
                                  Placement.Footprint.Width, Placement.Footprint.Height, Placement.Footprint.Depth);
            }
            else
            {
                // Tight row pitch is how much data to copy per row
                UINT TightRowPitch;
                CD3D11FormatHelper::CalculateMinimumRowMajorRowPitch(Placement.Footprint.Format, Placement.Footprint.Width, TightRowPitch);

                // Slice pitches are provided to enable fast paths which use a single memcpy
                UINT SrcSlicePitch = Dst.Parent()->ResourceDimension12() < D3D12_RESOURCE_DIMENSION_TEXTURE3D ?
                    (SrcData.SysMemPitch * Placement.Footprint.Height) : SrcData.SysMemSlicePitch;
                UINT DstSlicePitch;
                if (bDstBoxPresent)
                {
                    CD3D11FormatHelper::CalculateMinimumRowMajorSlicePitch(Placement.Footprint.Format,
                                                                            Placement.Footprint.RowPitch,
                                                                            Placement.Footprint.Height,
                                                                            DstSlicePitch);
                }
                else
                {
                    DstSlicePitch = Dst.DepthPitch(Subresource);
                }

                ImmediateContext::UploadDataToMappedBuffer(pSrcPlaneData, SrcData.SysMemPitch, SrcSlicePitch,
                                                            pDstSubresourceData, Placement.Footprint,
                                                            DstSlicePitch, TightRowPitch);

                pSrcPlaneData += SrcData.SysMemPitch * Placement.Footprint.Height;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CPrepareUpdateSubresourcesHelper::UploadDataToMappableResource(D3D11_SUBRESOURCE_DATA const* pSrcData, ImmediateContext& ImmCtx, D3D12_BOX const* pDstBox, const void* pClearPattern, UINT ClearPatternSize, UpdateSubresourcesFlags flags)
{
    // Now that we have something we can upload the data to, map it
    void* pDstData;
    const D3D12_RANGE ReadRange = {};
    ThrowFailure(mappableResource.Map(0, &ReadRange, &pDstData)); // throw( _com_error )

    // Now, upload the data for each subresource

    // The offset adjustment is subtracted from the offset of the given subresource, to calculate a location to write to
    // If we are doing UpdateSubresource on subresources 3 and 4, the offset to write to for subresource 4 is (offset of 4 - offset of 3)
    auto& FirstSubresourcePlacement = bUseLocalPlacement ? PreparedStorage.LocalPlacementDescs[0] : Dst.GetSubresourcePlacement(FirstDstSubresource);
    PreparedStorage.Base.OffsetAdjustment = FirstSubresourcePlacement.Offset;

    if (pSrcData != nullptr)
    {
        UploadSourceDataToMappableResource(pDstData, pSrcData, ImmCtx, flags);
    }
    else
    {
        // Just zero/fill the memory
        assert(TotalSize < size_t(-1));
        UINT64 CopySize = TotalSize;

        // If writing directly into the resource, we need to account for the dstBox instead of leaving it to the GPU copy
        if (!CachedNeedsTemporaryUploadHeap && pDstBox)
        {
            CopySize = min<UINT64>(CopySize, pDstBox->right - pDstBox->left);
        }

        if (pClearPattern)
        {
            assert(!CD3D11FormatHelper::Planar(Dst.AppDesc()->Format()) && CD3D11FormatHelper::GetBitsPerElement(Dst.AppDesc()->Format()) % 8 == 0);
            assert(NumDstSubresources == 1);
            // What we're clearing here may not be one pixel, so intentionally using GetByteAlignment to determine the minimum size
            // for a fully aligned block of pixels. (E.g. YUY2 is 8 bits per element * 2 elements per pixel * 2 pixel subsampling = 32 bits of clear data).
            const UINT SizeOfClearPattern = ClearPatternSize != 0 ? ClearPatternSize :
                CD3D11FormatHelper::GetByteAlignment(Dst.AppDesc()->Format());
            UINT ClearByteIndex = 0;
            auto generator = [&]()
            {
                auto result = *(reinterpret_cast<const BYTE*>(pClearPattern) + ClearByteIndex);
                ClearByteIndex = (ClearByteIndex + 1) % SizeOfClearPattern;
                return result;
            };
            if (FirstSubresourcePlacement.Footprint.RowPitch % SizeOfClearPattern != 0)
            {
                UINT SlicePitch;
                CD3D11FormatHelper::CalculateMinimumRowMajorSlicePitch(
                    FirstSubresourcePlacement.Footprint.Format,
                    FirstSubresourcePlacement.Footprint.RowPitch,
                    FirstSubresourcePlacement.Footprint.Height,
                    SlicePitch);

                // We need to make sure to leave a gap in the pattern so that it starts on byte 0 for every row
                for (UINT z = 0; z < FirstSubresourcePlacement.Footprint.Depth; ++z)
                {
                    for (UINT y = 0; y < FirstSubresourcePlacement.Footprint.Height; ++y)
                    {
                        BYTE* pDstRow = (BYTE*)pDstData + bufferOffset +
                            FirstSubresourcePlacement.Footprint.RowPitch * y +
                            SlicePitch * z;
                        ClearByteIndex = 0;
                        std::generate_n(pDstRow, FirstSubresourcePlacement.Footprint.RowPitch, generator);
                    }
                }
            }
            else
            {
                std::generate_n((BYTE*)pDstData + bufferOffset, CopySize, generator);
            }
        }
        else
        {
            ZeroMemory((BYTE *)pDstData + bufferOffset, static_cast<size_t>(CopySize));
        }
    }

    CD3DX12_RANGE WrittenRange(0, SIZE_T(TotalSize));
    mappableResource.Unmap(0, &WrittenRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CPrepareUpdateSubresourcesHelper::WriteOutputParameters(D3D12_BOX const* pDstBox, UpdateSubresourcesFlags flags)
{
    // Write output parameters
    UpdateSubresourcesFlags scenario = flags & UpdateSubresourcesFlags::ScenarioMask;
    if (pDstBox)
    {
        PreparedStorage.Base.DstX = pDstBox->left;
        PreparedStorage.Base.DstY = pDstBox->top;
        PreparedStorage.Base.DstZ = pDstBox->front;
    }
    else
    {
        PreparedStorage.Base.DstX = 0;
        PreparedStorage.Base.DstY = 0;
        PreparedStorage.Base.DstZ = 0;
    }
    PreparedStorage.Base.EncodedBlock = EncodedResourceSuballocation(mappableResource);
    PreparedStorage.Base.EncodedSubresourceSubset = Subresources;
    PreparedStorage.Base.bDisablePredication =
        (scenario == UpdateSubresourcesFlags::ScenarioInitialData || scenario == UpdateSubresourcesFlags::ScenarioImmediateContextInternalOp);
    PreparedStorage.Base.bDstBoxPresent = bDstBoxPresent;
}

//----------------------------------------------------------------------------------------------------------------------------------
_Use_decl_annotations_
void ImmediateContext::UpdateSubresources(Resource* pDst, D3D12TranslationLayer::CSubresourceSubset const& Subresources, const D3D11_SUBRESOURCE_DATA* pSrcData, const D3D12_BOX* pDstBox, UpdateSubresourcesFlags flags, const void* pClearColor )
{
    CPrepareUpdateSubresourcesHelper PrepareHelper(*pDst, Subresources, pSrcData, pDstBox, flags, pClearColor, 0, *this);
    if (PrepareHelper.FinalizeNeeded)
    {
        FinalizeUpdateSubresources(pDst, PrepareHelper.PreparedStorage.Base, PrepareHelper.bUseLocalPlacement ? PrepareHelper.PreparedStorage.LocalPlacementDescs : nullptr);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ResourceUpdateSubresourceUP(Resource* pResource, UINT DstSubresource, _In_opt_ const D3D12_BOX* pDstBox, _In_ const VOID* pMem, UINT SrcPitch, UINT SrcDepth)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);
    D3D11_SUBRESOURCE_DATA SubresourceDesc = { pMem, SrcPitch, SrcDepth };
    UINT8 MipLevel, PlaneSlice;
    UINT16 ArraySlice;
    DecomposeSubresourceIdxExtended(DstSubresource, pResource->AppDesc()->MipLevels(), pResource->AppDesc()->ArraySize(), MipLevel, ArraySlice, PlaneSlice);
    UpdateSubresources(pResource,
        CSubresourceSubset(1, 1, pResource->SubresourceMultiplier(), MipLevel, ArraySlice, PlaneSlice),
        &SubresourceDesc, pDstBox);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Calculate either a new coordinate in the same subresource, or targeting a new subresource with the number of tiles remaining
inline void CalcNewTileCoords(D3D12_TILED_RESOURCE_COORDINATE &Coord, UINT &NumTiles, D3D12_SUBRESOURCE_TILING const& SubresourceTiling)
{
    CalcNewTileCoords(reinterpret_cast<D3D11_TILED_RESOURCE_COORDINATE&>(Coord),
                      NumTiles,
                      reinterpret_cast<const D3D11_SUBRESOURCE_TILING&>(SubresourceTiling));
}

COMMAND_LIST_TYPE ImmediateContext::GetFallbackCommandListType(UINT commandListTypeMask)
{
    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "ImmediateContext::GetFallbackCommandListType must support all command list types.");

    COMMAND_LIST_TYPE fallbackList[] =
    {
        COMMAND_LIST_TYPE::GRAPHICS,
        COMMAND_LIST_TYPE::VIDEO_DECODE,
        COMMAND_LIST_TYPE::VIDEO_PROCESS,
    };

    for (auto type : fallbackList)
    {
        if (commandListTypeMask & (1 << (UINT)type))
        {
            return type;
        }
    }
    return COMMAND_LIST_TYPE::GRAPHICS;
}


//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UpdateTileMappings(
    Resource* pResource,
    UINT NumTiledResourceRegions,
    _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords,
    _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
    Resource* pTilePool,
    UINT NumRanges,
    _In_reads_opt_(NumRanges) const TILE_RANGE_FLAG* pRangeFlags,
    _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets,
    _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts,
    TILE_MAPPING_FLAG Flags)
{
    bool NeedToSubmit = true;
    UINT commandListTypeMask = pResource->GetCommandListTypeMask();
    if (commandListTypeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
    {
        commandListTypeMask = COMMAND_LIST_TYPE_GRAPHICS_MASK;      // fallback to graphics
        NeedToSubmit = false;
    }

    // if we have subresources appearing in different command list types, we need to synchronize to a target command list and then submit the operation on the target one.
    COMMAND_LIST_TYPE targetListType = GetFallbackCommandListType(commandListTypeMask);
    UINT targetListMask = 1 << (UINT)targetListType;
    if (!IsSingleCommandListType(commandListTypeMask))
    {
        for (UINT subresource = 0; subresource < pResource->NumSubresources(); subresource++)
        {
            if (pResource->GetCommandListTypeMask(subresource) != targetListMask)
            {
                m_ResourceStateManager.TransitionSubresource(pResource, subresource, D3D12_RESOURCE_STATE_COMMON, targetListType, SubresourceTransitionFlags::ForceExclusiveState);
            }
        }
        m_ResourceStateManager.ApplyAllResourceTransitions();
    }

    UpdateTileMappingsImpl(targetListType, pResource, NumTiledResourceRegions, pTiledResourceRegionStartCoords, pTiledResourceRegionSizes, pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets, pRangeTileCounts, Flags, NeedToSubmit);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::UpdateTileMappingsImpl(
    COMMAND_LIST_TYPE commandListType,
    Resource* pResource,
    UINT NumTiledResourceRegions,
    _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords,
    _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
    Resource* pTilePool,
    UINT NumRanges,
    _In_reads_opt_(NumRanges) const TILE_RANGE_FLAG* pRangeFlags,
    _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets,
    _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts,
    TILE_MAPPING_FLAG Flags,
    bool NeedToSubmit)
{
    // Helper methods
    auto pfnGetAllocationForTile = [pTilePool](UINT Tile) -> Resource::STilePoolAllocation&
    {
        const UINT BytesPerTile = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
        const UINT ByteOffset = Tile * BytesPerTile;
        UINT CurrBytes = 0;
        for (auto& Allocation : pTilePool->m_TilePool.m_Allocations)
        {
            CurrBytes += Allocation.m_Size;
            if (ByteOffset < CurrBytes)
                return Allocation;
        }

        ASSUME(false);
    };

    // This code still honors the D3D11 tiled resource tier 1 restriction, but does so only on D3D12 resource heap tier 2 or above.
    // Yes, they are related; but they aren't yet fully teased apart.
    // The tiled resource tier 1 restriction: one physical page cannot be mapped to buffer & texture simulatenously.
    // D3D12 resource heap tier 1 precludes one physical page from being mapped to three types of resources simulatenously.
    assert(m_caps.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2 );
    const bool bTier1 = m_caps.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_1;
    const bool bTexture = bTier1 && pResource->Parent()->ResourceDimension12() != D3D12_RESOURCE_DIMENSION_BUFFER;
    auto pfnGetHeapForAllocation = [=](Resource::STilePoolAllocation& Allocation) -> ID3D12Heap*
    {
        auto& spHeap = bTexture ? Allocation.m_spUnderlyingTextureHeap : Allocation.m_spUnderlyingBufferHeap;
        if (!spHeap)
        {
            CD3DX12_HEAP_DESC Desc(Allocation.m_Size, GetHeapProperties(D3D12_HEAP_TYPE_DEFAULT));
            
            HRESULT hr = m_pDevice12->CreateHeap(
                &Desc,
                IID_PPV_ARGS(&spHeap) );
            ThrowFailure(hr);
        }
        return spHeap.get();
    };

    if (NeedToSubmit &&  (Flags & TILE_MAPPING_NO_OVERWRITE) == 0 && HasCommands(commandListType))
    {
        SubmitCommandList(commandListType);  // throws
    }

    pResource->UsedInCommandList(commandListType, GetCommandListID(commandListType));
    GetCommandListManager(commandListType)->ExecuteCommandQueueCommand([&]()
    {
        UINT NumStandardMips = pResource->m_TiledResource.m_NumStandardMips;
        UINT NumTilesRequired = pResource->m_TiledResource.m_NumTilesForResource;
        bool bPackedMips = NumStandardMips != pResource->AppDesc()->MipLevels();

        if (pTilePool)
        {
            if (pTilePool != pResource->m_TiledResource.m_pTilePool && pResource->m_TiledResource.m_pTilePool != nullptr)
            {
                // Unmap all tiles from the old tile pool
                static const D3D12_TILE_RANGE_FLAGS NullFlag = D3D12_TILE_RANGE_FLAG_NULL;
                static const D3D12_TILED_RESOURCE_COORDINATE StartCoords = {};
                const D3D12_TILE_REGION_SIZE FullResourceSize = {NumTilesRequired};
                GetCommandQueue(commandListType)->UpdateTileMappings(
                    pResource->GetUnderlyingResource(),
                    1, // Number of regions
                    &StartCoords,
                    &FullResourceSize,
                    nullptr, // Tile pool (can be null when unbinding)
                    1, // Number of ranges
                    &NullFlag,
                    nullptr, // Tile pool start (ignored when flag is null)
                    &NumTilesRequired,
                    D3D12_TILE_MAPPING_FLAGS( Flags ));
            }

            pResource->m_TiledResource.m_pTilePool = pTilePool;
        }

        // UpdateTileMappings is 1:1 with D3D12 if the tile pool has never been grown,
        // OR if the entire region of tiles comes from the first allocation (heap)

        // First: Does the entire range fit into one allocation (or does it not need any allocation references)?
        // Trivially true if we don't have a tile pool, or if the tile pool only has one allocation
        bool bNoAllocations = !pTilePool;
        bool bOneOrNoAllocations = bNoAllocations || pTilePool->m_TilePool.m_Allocations.size() == 1;

        // Trivial check says no - we can't pass null to 12, and we can't pass the 11 parameters straight through
        // Now we need to figure out if the tile ranges being specified are all really from the same allocation
        if (!bOneOrNoAllocations)
        {
            // Assume that we won't find a reference to another allocation
            bOneOrNoAllocations = true;
            bNoAllocations = true;
            assert(pTilePool);

            UINT Allocation0NumTiles = pTilePool->m_TilePool.m_Allocations.front().m_Size / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
            for (UINT range = 0; range < NumRanges; ++range)
            {
                UINT RangeFlag = pRangeFlags ? pRangeFlags[range] : 0;
                if (RangeFlag == 0 || RangeFlag == TILE_RANGE_REUSE_SINGLE_TILE)
                {
                    // We're definitely binding a tile
                    bNoAllocations = false;
                    UINT BaseTile = pTilePoolStartOffsets[range];
                    UINT EndTile = BaseTile + ((RangeFlag == 0 && pRangeTileCounts) ? pRangeTileCounts[range] : 1);
                    if (EndTile > Allocation0NumTiles)
                    {
                        // And it's not the first one
                        bOneOrNoAllocations = false;
                        break;
                    }
                }
            }
        }

        // Now we know how for sure how to translate this to 12
        // If the first or no allocations, we can pass the 11 parameters straight through
        // (if no allocations, we can avoid lazy instantiation of per-kind heaps for tier 1)
        // Otherwise, we need to split this up into multiple API invocations

        if (bOneOrNoAllocations)
        {
            auto pCoord = reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE*>(pTiledResourceRegionStartCoords);
            auto pSize = reinterpret_cast<const D3D12_TILE_REGION_SIZE*>(pTiledResourceRegionSizes);
            ID3D12Heap *pHeap = nullptr;
            if (!bNoAllocations)
            {
                pHeap = pfnGetHeapForAllocation(
                    pfnGetAllocationForTile(pTilePoolStartOffsets[0])); // throw( _com_error )
            }
            GetCommandQueue(commandListType)->UpdateTileMappings(
                pResource->GetUnderlyingResource(),
                NumTiledResourceRegions,
                pCoord,
                pSize,
                pHeap,
                NumRanges,
                reinterpret_cast<const D3D12_TILE_RANGE_FLAGS*>(pRangeFlags),
                pTilePoolStartOffsets,
                pRangeTileCounts,
                D3D12_TILE_MAPPING_FLAGS(Flags) );
        }
        else
        {
            assert(pTilePool);

            // For each resource region or tile region, submit an UpdateTileMappings op
            D3D12_TILED_RESOURCE_COORDINATE Coord;
            D3D12_TILE_REGION_SIZE Size;
            D3D12_TILE_RANGE_FLAGS Flag = pRangeFlags ?
                    static_cast<D3D12_TILE_RANGE_FLAGS>(pRangeFlags[0]) : D3D12_TILE_RANGE_FLAG_NONE;

            UINT range = 0, region = 0;

            UINT CurrTile = pTilePoolStartOffsets[0];
            UINT NumTiles = pRangeTileCounts ? pRangeTileCounts[0] : 0xffffffff;

            Coord = pTiledResourceRegionStartCoords ? reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE&>(pTiledResourceRegionStartCoords[0]) : D3D12_TILED_RESOURCE_COORDINATE{};
            Size = pTiledResourceRegionSizes ? reinterpret_cast<const D3D12_TILE_REGION_SIZE&>(pTiledResourceRegionSizes[0]) :
                (pTiledResourceRegionStartCoords ? D3D12_TILE_REGION_SIZE{1, FALSE} : D3D12_TILE_REGION_SIZE{NumTilesRequired, FALSE});

            D3D12_BOX CurrentBox = {};
            bool bBox = false;

            while(range < NumRanges && region < NumTiledResourceRegions)
            {
                // Step 1: Figure out what will determine the bounds of this particular update: the region, the range, or the heap
                UINT NumTilesForRegion = Size.NumTiles;
                UINT NumTilesForRange = NumTiles;

                UINT NumTilesToUpdate = min(NumTilesForRegion, NumTilesForRange);

                auto &Allocation = pfnGetAllocationForTile(CurrTile);

                // If we are dealing with multiple tiles from the pool, does the current heap have enough space for it?
                if (Flag == D3D12_TILE_RANGE_FLAG_NONE)
                {
                    UINT NumTilesInHeap = Allocation.m_Size / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES
                        + Allocation.m_TileOffset - CurrTile;
                    NumTilesToUpdate = min(NumTilesToUpdate, NumTilesInHeap);
                }

                // If the app wanted to use a box, but the region was not the smallest unit here, we need to break up the region
                // The simplest way to do that is to break it up into 1x1 regions, so we set that up here
                if (NumTilesToUpdate != NumTilesForRegion && Size.UseBox)
                {
                    CurrentBox = {Coord.X, Coord.Y, Coord.Z,
                        Coord.X + Size.Width, Coord.Y + Size.Height, Coord.Z + Size.Depth};
                    bBox = true;
                    Size = {1, false};

                    NumTilesForRegion = 1;
                    NumTilesToUpdate = 1;
                }

                // Step 2: Actually issue the update operation (if this range isn't being skipped)
                if (Flag != D3D12_TILE_RANGE_FLAG_SKIP)
                {
                    D3D12_TILE_REGION_SIZE APISize = {NumTilesToUpdate, FALSE};

                    ID3D12Heap *pHeap = Flag == D3D12_TILE_RANGE_FLAG_NULL ? nullptr : pfnGetHeapForAllocation(Allocation); // throw( _com_error )
                    UINT BaseTile = CurrTile - Allocation.m_TileOffset;
                    GetCommandQueue(commandListType)->UpdateTileMappings(
                        pResource->GetUnderlyingResource(),
                        1,
                        &Coord,
                        &APISize,
                        pHeap,
                        1,
                        &Flag,
                        &BaseTile,
                        &NumTilesToUpdate,
                        D3D12_TILE_MAPPING_FLAGS(Flags) );
                }

                // Step 3: Advance the iteration structs
                // Start with the tiled resource region
                if (NumTilesToUpdate == NumTilesForRegion)
                {
                    // First, flow through the box
                    bool bAdvanceRegion = !bBox;
                    if (bBox)
                    {
                        ++Coord.X;
                        if (Coord.X == CurrentBox.right)
                        {
                            Coord.X = CurrentBox.left;
                            ++Coord.Y;
                            if (Coord.Y == CurrentBox.bottom)
                            {
                                Coord.Y = CurrentBox.top;
                                ++Coord.Z;
                                if (Coord.Z == CurrentBox.back)
                                {
                                    bBox = false;
                                    bAdvanceRegion = true;
                                }
                            }
                        }
                    }

                    // If we don't have a box, or we finished the box, then go to the next region
                    if (bAdvanceRegion && ++region < NumTiledResourceRegions)
                    {
                        assert(pTiledResourceRegionStartCoords);
                        Coord = reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE&>(pTiledResourceRegionStartCoords[region]);
                        Size = pTiledResourceRegionSizes ? reinterpret_cast<const D3D12_TILE_REGION_SIZE&>(pTiledResourceRegionSizes[region]) : D3D12_TILE_REGION_SIZE{1, FALSE};
                    }
                }
                else
                {
                    assert(!bBox);
                    Size.NumTiles -= NumTilesToUpdate;
                    
                    // Calculate a new region based on tile flow across dimensions/mips
                    UINT TempTileCount = NumTilesToUpdate;
                    while (TempTileCount)
                    {
                        if (bPackedMips && Coord.Subresource >= NumStandardMips)
                        {
                            Coord.Subresource = NumStandardMips;
                            Coord.X += TempTileCount;
                            break;
                        }
                        else
                        {
                            D3D12_SUBRESOURCE_TILING const& SubresourceTiling =
                                pResource->m_TiledResource.m_SubresourceTiling[Coord.Subresource % pResource->AppDesc()->MipLevels()];
                            CalcNewTileCoords(Coord, TempTileCount, SubresourceTiling);
                        }
                    }
                }

                // Then the tile pool range
                if (NumTilesToUpdate == NumTilesForRange)
                {
                    if (++range < NumRanges)
                    {
                        assert(pRangeTileCounts);
                        Flag = pRangeFlags ? static_cast<D3D12_TILE_RANGE_FLAGS>(pRangeFlags[range]) : D3D12_TILE_RANGE_FLAG_NONE;
                        CurrTile = Flag == D3D12_TILE_RANGE_FLAG_NULL ? 0 : pTilePoolStartOffsets[range];
                        NumTiles = pRangeTileCounts[range];
                    }
                }
                else
                {
                    if (Flag == D3D12_TILE_RANGE_FLAG_NONE)
                    {
                        CurrTile += NumTilesToUpdate;
                    }
                    NumTiles -= NumTilesToUpdate;
                }
            }
        }
    });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CopyTileMappings(Resource* pDstTiledResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstStartCoords, Resource* pSrcTiledResource, _In_ const  D3D12_TILED_RESOURCE_COORDINATE* pSrcStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, TILE_MAPPING_FLAG Flags)
{
    UINT commandListTypeMask = pSrcTiledResource->GetCommandListTypeMask() | pDstTiledResource->GetCommandListTypeMask();
    if (commandListTypeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
    {
        commandListTypeMask = COMMAND_LIST_TYPE_GRAPHICS_MASK;      // fallback to graphics
    }

    // if we have subresources appearing in different command list types, we need to synchronize to a target command list and then submit the operation on the target one.
    COMMAND_LIST_TYPE targetListType = GetFallbackCommandListType(commandListTypeMask);
    UINT targetListMask = 1 << (UINT)targetListType;
    if (!IsSingleCommandListType(commandListTypeMask))
    {
        for (UINT subresource = 0; subresource < pSrcTiledResource->NumSubresources(); subresource++)
        {
            if (pSrcTiledResource->GetCommandListTypeMask(subresource) != targetListMask)
            {
                m_ResourceStateManager.TransitionSubresource(pSrcTiledResource, subresource, D3D12_RESOURCE_STATE_COMMON, targetListType, SubresourceTransitionFlags::ForceExclusiveState);
            }
        }
        for (UINT subresource = 0; subresource < pDstTiledResource->NumSubresources(); subresource++)
        {
            if (pDstTiledResource->GetCommandListTypeMask(subresource) != targetListMask)
            {
                m_ResourceStateManager.TransitionSubresource(pDstTiledResource, subresource, D3D12_RESOURCE_STATE_COMMON, targetListType, SubresourceTransitionFlags::ForceExclusiveState);
            }
        }
        m_ResourceStateManager.ApplyAllResourceTransitions();
    }

    CopyTileMappingsImpl(targetListType, pDstTiledResource, pDstStartCoords, pSrcTiledResource, pSrcStartCoords, pTileRegion, Flags);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CopyTileMappingsImpl(COMMAND_LIST_TYPE commandListType, Resource* pDstTiledResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstStartCoords, Resource* pSrcTiledResource, _In_ const  D3D12_TILED_RESOURCE_COORDINATE* pSrcStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, TILE_MAPPING_FLAG Flags )
{
    auto pDst = pDstTiledResource->GetUnderlyingResource();
    auto pSrc = pSrcTiledResource->GetUnderlyingResource();
    if ((Flags & TILE_MAPPING_NO_OVERWRITE) == 0 && HasCommands(commandListType))
    {
        SubmitCommandList(commandListType); // throws
    }

    pDstTiledResource->UsedInCommandList(commandListType, GetCommandListID(commandListType));
    pSrcTiledResource->UsedInCommandList(commandListType, GetCommandListID(commandListType));
    GetCommandListManager(commandListType)->ExecuteCommandQueueCommand([&]()
    {
        auto pTilePool = pSrcTiledResource->m_TiledResource.m_pTilePool;
        if (pTilePool != pDstTiledResource->m_TiledResource.m_pTilePool && pDstTiledResource->m_TiledResource.m_pTilePool != nullptr)
        {
            UINT NumTilesRequired = pDstTiledResource->m_TiledResource.m_NumTilesForResource;

            // Unmap all tiles from the old tile pool
            static const D3D12_TILE_RANGE_FLAGS NullFlag = D3D12_TILE_RANGE_FLAG_NULL;
            static const D3D12_TILED_RESOURCE_COORDINATE StartCoords = {};
            const D3D12_TILE_REGION_SIZE FullResourceSize = { NumTilesRequired };
            GetCommandQueue(commandListType)->UpdateTileMappings(
                pDstTiledResource->GetUnderlyingResource(),
                1, // Number of regions
                &StartCoords,
                &FullResourceSize,
                nullptr, // Tile pool (can be null when unbinding)
                1, // Number of ranges
                &NullFlag,
                nullptr, // Tile pool start (ignored when flag is null)
                &NumTilesRequired,
                D3D12_TILE_MAPPING_FLAGS(Flags));
        }
        pDstTiledResource->m_TiledResource.m_pTilePool = pTilePool;

        GetCommandQueue(commandListType)->CopyTileMappings(pDst,
                                                           reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE*>(pDstStartCoords),
                                                           pSrc,
                                                           reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE*>(pSrcStartCoords),
                                                           reinterpret_cast<const D3D12_TILE_REGION_SIZE*>(pTileRegion),
                                                           D3D12_TILE_MAPPING_FLAGS(Flags));
    });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CopyTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, Resource* pBuffer, UINT64 BufferOffset, TILE_COPY_FLAG Flags)
{
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);
    Resource *pSrc, *pDst;

    D3D12_RESOURCE_STATES StateForTiledResource;

    if (Flags & TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE)
    {
        StateForTiledResource = D3D12_RESOURCE_STATE_COPY_DEST;
        m_ResourceStateManager.TransitionResource(pBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        pSrc = pBuffer;
        pDst = pResource;
    }
    else
    {
        assert(Flags & TILE_COPY_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER);
        StateForTiledResource = D3D12_RESOURCE_STATE_COPY_SOURCE;
        m_ResourceStateManager.TransitionResource(pBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
        pSrc = pResource;
        pDst = pBuffer;
    }

    {
        CTileSubresourceSubset TileSubset(
            *reinterpret_cast<const D3D11_TILED_RESOURCE_COORDINATE*>(pStartCoords),
            *reinterpret_cast<const D3D11_TILE_REGION_SIZE*>(pTileRegion),
            pResource->Parent()->ResourceDimension11(),
            reinterpret_cast<const D3D11_SUBRESOURCE_TILING*>(pResource->m_TiledResource.m_SubresourceTiling.begin()),
            pResource->AppDesc()->MipLevels(),
            pResource->m_TiledResource.m_NumStandardMips);
        for (UINT Subresource : TileSubset)
        {
            m_ResourceStateManager.TransitionSubresource(pResource, Subresource, StateForTiledResource);
        }
    }

    m_ResourceStateManager.ApplyAllResourceTransitions();

    auto pAPIResource = pResource->GetUnderlyingResource();
    auto pAPIBuffer = pBuffer->GetUnderlyingResource();
    GetGraphicsCommandList()->CopyTiles(pAPIResource,
                                     reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE*>(pStartCoords),
                                     reinterpret_cast<const D3D12_TILE_REGION_SIZE*>(pTileRegion),
                                     pAPIBuffer,
                                     BufferOffset,
                                     D3D12_TILE_COPY_FLAGS(Flags));

    PostCopy(pSrc, 0, pDst, 0, pSrc->NumSubresources());
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UpdateTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pCoord, _In_ const D3D12_TILE_REGION_SIZE* pRegion, const _In_ VOID* pData, UINT Flags)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"UpdateTiles");
#endif
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);
    
    pResource->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

    {
        CTileSubresourceSubset TileSubset(
            *reinterpret_cast<const D3D11_TILED_RESOURCE_COORDINATE*>(pCoord),
            *reinterpret_cast<const D3D11_TILE_REGION_SIZE*>(pRegion),
            pResource->Parent()->ResourceDimension11(),
            reinterpret_cast<const D3D11_SUBRESOURCE_TILING*>(pResource->m_TiledResource.m_SubresourceTiling.begin()),
            pResource->AppDesc()->MipLevels(),
            pResource->m_TiledResource.m_NumStandardMips);
        for (UINT Subresource : TileSubset)
        {
            m_ResourceStateManager.TransitionSubresource(pResource, Subresource, D3D12_RESOURCE_STATE_COPY_DEST);
        }
    }

    m_ResourceStateManager.ApplyAllResourceTransitions();

    UINT64 DataSize = (UINT64)pRegion->NumTiles * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
    auto UploadHeap = AcquireSuballocatedHeap(AllocatorHeapType::Upload, DataSize, ResourceAllocationContext::ImmediateContextThreadTemporary); // throw( _com_error )

    void* pMapped;
    CD3DX12_RANGE ReadRange(0, 0);
    HRESULT hr = UploadHeap.Map(0, &ReadRange, &pMapped);
    ThrowFailure(hr); // throw( _com_error )

    assert(DataSize < (SIZE_T)-1); // Can't map a buffer whose size is more than size_t
    memcpy(pMapped, pData, SIZE_T(DataSize));

    CD3DX12_RANGE WrittenRange(0, SIZE_T(DataSize));
    UploadHeap.Unmap(0, &WrittenRange);

    GetGraphicsCommandList()->CopyTiles(
        pResource->GetUnderlyingResource(),
        reinterpret_cast<const D3D12_TILED_RESOURCE_COORDINATE*>(pCoord),
        reinterpret_cast<const D3D12_TILE_REGION_SIZE*>(pRegion),
        UploadHeap.GetResource(),
        UploadHeap.GetOffset(),
        D3D12_TILE_COPY_FLAGS(Flags) | D3D12_TILE_COPY_FLAG_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE
        );

    ReleaseSuballocatedHeap(AllocatorHeapType::Upload, UploadHeap, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS), COMMAND_LIST_TYPE::GRAPHICS);

    PostUpload();
}


//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::TiledResourceBarrier(Resource* pBefore, Resource* pAfter)
{
    UINT commandListTypeMask = COMMAND_LIST_TYPE_UNKNOWN_MASK;
    if (pAfter)
    {
        commandListTypeMask = pAfter->GetCommandListTypeMask();
    }

    if (pBefore)
    {
        commandListTypeMask |= pBefore->GetCommandListTypeMask();
    }

    // defaulting to graphics explicitly
    if (commandListTypeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
    {
        commandListTypeMask = COMMAND_LIST_TYPE_GRAPHICS_MASK;
    }

    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if (commandListTypeMask & (1 << i))
        {
            TiledResourceBarrierImpl((COMMAND_LIST_TYPE)i, pBefore, pAfter);
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::TiledResourceBarrierImpl(COMMAND_LIST_TYPE commandListType, Resource* pBefore, Resource* pAfter)
{
    PreRender(commandListType);
    D3D12_RESOURCE_BARRIER barrierDesc = {};
    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barrierDesc.Aliasing.pResourceBefore = (pBefore) ? pBefore->GetUnderlyingResource() : nullptr;
    barrierDesc.Aliasing.pResourceAfter = (pAfter) ? pAfter->GetUnderlyingResource() : nullptr;

    static_assert(static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID) == 3u, "ImmediateContext::TiledResourceBarrier must support all command list types.");

    switch (commandListType)
    {
    case COMMAND_LIST_TYPE::GRAPHICS:
        GetGraphicsCommandList()->ResourceBarrier(1, &barrierDesc);
        break;

    case COMMAND_LIST_TYPE::VIDEO_DECODE:
        GetVideoDecodeCommandList()->ResourceBarrier(1, &barrierDesc);
        break;

    case COMMAND_LIST_TYPE::VIDEO_PROCESS:
        GetVideoProcessCommandList()->ResourceBarrier(1, &barrierDesc);
        break;

    default:
        assert(0);
    }

    if (pBefore)
    {
        pBefore->UsedInCommandList(commandListType, GetCommandListID(commandListType));
    }

    if (pAfter)
    {
        pAfter->UsedInCommandList(commandListType, GetCommandListID(commandListType));
    }

    PostRender(commandListType);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ResizeTilePool(Resource* pResource, UINT64 NewSize )
{
    // For simplicity, tile pools in 11on12 are grow-only, since decrementing refs during tile mapping operations would be prohibitively expensive
    UINT64 CurrentSize = 0;
    for (auto& Allocation : pResource->m_TilePool.m_Allocations)
    {
        CurrentSize += Allocation.m_Size;
        if (CurrentSize >= NewSize)
            return; // Done
    }

    static const UINT64 Alignment = 1024*1024*4;
    static_assert(!(Alignment & (Alignment - 1)), "Alignment must be a power of 2");

    UINT64 SizeDiff = NewSize - CurrentSize;
    SizeDiff = Align(SizeDiff, Alignment); // Each additional tile pool will be a multiple of 4MB

    assert(SizeDiff < (UINT)-1);
    pResource->m_TilePool.m_Allocations.emplace_back(UINT(SizeDiff), UINT(CurrentSize / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES)); // throw( bad_alloc )

    auto TiledResourcesTier = m_caps.TiledResourcesTier;
    if (TiledResourcesTier != D3D12_TILED_RESOURCES_TIER_1)
    {
        auto& Allocation = pResource->m_TilePool.m_Allocations.back();

        CD3DX12_HEAP_DESC HeapDesc(SizeDiff, GetHeapProperties(D3D12_HEAP_TYPE_DEFAULT));
        HRESULT hr = m_pDevice12->CreateHeap(
            &HeapDesc,
            IID_PPV_ARGS(&Allocation.m_spUnderlyingBufferHeap) );

        ThrowFailure(hr);
    }
}

unique_comptr<ID3D12Resource> ImmediateContext::AcquireTransitionableUploadBuffer(AllocatorHeapType HeapType, UINT64 Size) noexcept(false)
{
    TDynamicBufferPool& Pool = GetBufferPool(HeapType);
    auto pfnCreateNew = [this, HeapType](UINT64 Size) -> unique_comptr<ID3D12Resource> // noexcept(false)
    {
        return std::move(AllocateHeap(Size, 0, HeapType));
    };

    UINT64 CurrentFence = GetCompletedFenceValue(CommandListType(HeapType));

    return Pool.RetrieveFromPool(Size, CurrentFence, pfnCreateNew); // throw( _com_error )
}

D3D12ResourceSuballocation ImmediateContext::AcquireSuballocatedHeapForResource(_In_ Resource* pResource, ResourceAllocationContext threadingContext) noexcept(false)
{
    UINT64 ResourceSize = pResource->GetResourceSize();
    
    // SRV buffers do not allow offsets to be specified in bytes but instead by number of Elements. This requires that the offset must 
    // always be aligned to an element size, which cannot be predicted since buffers can be created as DXGI_FORMAT_UNKNOWN and SRVs 
    // can later be created later with an arbitrary DXGI_FORMAT. To handle this, we don't allow a suballocated offset for this case
    bool bCannotBeOffset = (pResource->AppDesc()->BindFlags() & RESOURCE_BIND_SHADER_RESOURCE) && (pResource->AppDesc()->ResourceDimension() == D3D12_RESOURCE_DIMENSION_BUFFER);
    
    AllocatorHeapType HeapType = pResource->GetAllocatorHeapType();
    return AcquireSuballocatedHeap(HeapType, ResourceSize, threadingContext, bCannotBeOffset); // throw( _com_error )
}

//----------------------------------------------------------------------------------------------------------------------------------
D3D12ResourceSuballocation ImmediateContext::AcquireSuballocatedHeap(AllocatorHeapType HeapType, UINT64 Size, ResourceAllocationContext threadingContext, bool bCannotBeOffset) noexcept(false)
{
    if (threadingContext == ResourceAllocationContext::ImmediateContextThreadTemporary)
    {
        UploadHeapSpaceAllocated(CommandListType(HeapType), Size);
    }

    auto &allocator = GetAllocator(HeapType);
    
    HeapSuballocationBlock suballocation =
        TryAllocateResourceWithFallback([&]()
    {
        auto block = allocator.Allocate(Size, bCannotBeOffset);
        if (block.GetSize() == 0)
        {
            throw _com_error(E_OUTOFMEMORY);
        }
        return block;
    }, threadingContext);

    return D3D12ResourceSuballocation(allocator.GetInnerAllocation(suballocation), suballocation);
}

inline bool IsSyncPointLessThanOrEqual(UINT64(&lhs)[(UINT)COMMAND_LIST_TYPE::MAX_VALID],
                                       UINT64(&rhs)[(UINT)COMMAND_LIST_TYPE::MAX_VALID])
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
    {
        if (lhs[i] > rhs[i])
            return false;
    }
    return true;
}
//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::ResourceAllocationFallback(ResourceAllocationContext threadingContext)
{
    if (TrimDeletedObjects())
    {
        return true;
    }

    UINT64 SyncPoints[2][(UINT)COMMAND_LIST_TYPE::MAX_VALID];
    bool SyncPointExists[2];
    {
        auto DeletionManagerLocked = m_DeferredDeletionQueueManager.GetLocked();
        SyncPointExists[0] = DeletionManagerLocked->GetFenceValuesForObjectDeletion(SyncPoints[0]);
        SyncPointExists[1] = DeletionManagerLocked->GetFenceValuesForSuballocationDeletion(SyncPoints[1]);
    }
    // If one is strictly less than the other, wait just for that one.
    if (SyncPointExists[0] && SyncPointExists[1])
    {
        if (IsSyncPointLessThanOrEqual(SyncPoints[0], SyncPoints[1]))
        {
            SyncPointExists[1] = false;
        }
        else if (IsSyncPointLessThanOrEqual(SyncPoints[1], SyncPoints[0]))
        {
            SyncPointExists[0] = false;
        }
    }
    const bool ImmediateContextThread = threadingContext != ResourceAllocationContext::FreeThread;
    auto WaitForSyncPoint = [&](UINT64(&SyncPoint)[(UINT)COMMAND_LIST_TYPE::MAX_VALID])
    {
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            auto CommandListManager = GetCommandListManager((COMMAND_LIST_TYPE)i);
            if (CommandListManager)
            {
                CommandListManager->WaitForFenceValueInternal(ImmediateContextThread, SyncPoint[i]); // throws
            }
        }
    };

    //If index == 0 we are checking for object deletion, else subobject deletion
    auto WasMemoryFreed = [&](int index) -> bool {
        // DeferredDeletionQueueManager::TrimDeletedObjects() is the only place where we pop() 
        // items from the deletion queues. This means that, if the sync points are different after
        // the WaitForSyncPoint call, we must have called TrimDeletedObjects and freed some memory. 
        UINT64 newSyncPoint[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        auto DeletionManagerLocked = m_DeferredDeletionQueueManager.GetLocked();
        bool newSyncPointExists = false;
        if (index == 0) {
            newSyncPointExists = DeletionManagerLocked->GetFenceValuesForObjectDeletion(newSyncPoint);
        }
        else {
            newSyncPointExists = DeletionManagerLocked->GetFenceValuesForSuballocationDeletion(newSyncPoint);
        }
        constexpr size_t numBytes = sizeof(UINT64) * (size_t)COMMAND_LIST_TYPE::MAX_VALID;
        return !newSyncPointExists || memcmp(&SyncPoints[index], &newSyncPoint, numBytes);
    };

    bool freedMemory = false;
    if (SyncPointExists[0])
    {
        WaitForSyncPoint(SyncPoints[0]); // throws
        freedMemory = WasMemoryFreed(0);
    }
    if (SyncPointExists[1])
    {
        WaitForSyncPoint(SyncPoints[1]); // throws
        freedMemory |= WasMemoryFreed(1);
    }

    // If we've already freed up memory go ahead and return true, else try to Trim now and return that result
    return freedMemory || TrimDeletedObjects();
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ReturnAllBuffersToPool(Resource& UnderlyingResource) noexcept
{
    if (!UnderlyingResource.m_Identity)
    {
        return;
    }

    if (!UnderlyingResource.m_Identity->m_bOwnsUnderlyingResource)
    {
        assert(UnderlyingResource.m_Identity->m_spUnderlyingResource.get() == nullptr);
        AllocatorHeapType HeapType = UnderlyingResource.GetAllocatorHeapType();

        if (!UnderlyingResource.m_Identity->m_suballocation.IsInitialized())
        {
            return;
        }

        assert(UnderlyingResource.AppDesc()->CPUAccessFlags() != 0);

        ReleaseSuballocatedHeap(
            HeapType, 
            UnderlyingResource.m_Identity->m_suballocation,
            UnderlyingResource.m_LastUsedCommandListID);
    }
}


// This is for cases where we're copying a small subrect from one surface to another,
// specifically when we only want to copy the first part of each row. 
void MemcpySubresourceWithCopySize(
    _In_ const D3D12_MEMCPY_DEST* pDest,
    _In_ const D3D12_SUBRESOURCE_DATA* pSrc,
    SIZE_T /*RowSizeInBytes*/,
    UINT CopySize,
    UINT NumRows,
    UINT NumSlices)
{
    for (UINT z = 0; z < NumSlices; ++z)
    {
        BYTE* pDestSlice = reinterpret_cast<BYTE*>(pDest->pData) + pDest->SlicePitch * z;
        const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch * z;
        for (UINT y = 0; y < NumRows; ++y)
        {
            memcpy(pDestSlice + pDest->RowPitch * y,
                pSrcSlice + pSrc->RowPitch * y,
                CopySize);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ReturnTransitionableBufferToPool(AllocatorHeapType HeapType, UINT64 Size, unique_comptr<ID3D12Resource>&& spResource, UINT64 FenceValue) noexcept
{
    TDynamicBufferPool& Pool = GetBufferPool(HeapType);

    Pool.ReturnToPool(
        Size,
        std::move(spResource),
        FenceValue);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ReleaseSuballocatedHeap(AllocatorHeapType HeapType, D3D12ResourceSuballocation &resource, UINT64 FenceValue, COMMAND_LIST_TYPE commandListType) noexcept
{
    auto &allocator = GetAllocator(HeapType);

    m_DeferredDeletionQueueManager.GetLocked()->AddSuballocationToQueue(resource.GetBufferSuballocation(), allocator, commandListType, FenceValue);
    resource.Reset();
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ReleaseSuballocatedHeap(AllocatorHeapType HeapType, D3D12ResourceSuballocation &resource, const UINT64 FenceValues[]) noexcept
{
    auto &allocator = GetAllocator(HeapType);

    m_DeferredDeletionQueueManager.GetLocked()->AddSuballocationToQueue(resource.GetBufferSuballocation(), allocator, FenceValues);
    resource.Reset();
}

//----------------------------------------------------------------------------------------------------------------------------------
Resource* TRANSLATION_API ImmediateContext::CreateRenameCookie(Resource* pResource, ResourceAllocationContext threadingContext)
{
    assert(pResource->GetEffectiveUsage() == RESOURCE_USAGE_DYNAMIC);

    auto creationArgsCopy = pResource->m_creationArgs;
    creationArgsCopy.m_appDesc.m_usage = RESOURCE_USAGE_STAGING; // Make sure it's suballocated
    creationArgsCopy.m_heapDesc.Properties = CD3DX12_HEAP_PROPERTIES(Resource::GetD3D12HeapType(RESOURCE_USAGE_STAGING, creationArgsCopy.m_appDesc.CPUAccessFlags()), GetNodeMask(), GetNodeMask());

    // Strip video flags which don't make sense on the staging buffer in D3D12 and trip up allocation logic that follows.
    creationArgsCopy.m_appDesc.m_bindFlags &= ~(RESOURCE_BIND_DECODER | RESOURCE_BIND_VIDEO_ENCODER | RESOURCE_BIND_CAPTURE);
    creationArgsCopy.m_flags11.BindFlags &= ~(D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER);

    // Inherit the heap type from from the previous resource (which may account for the video flags stripped above).
    creationArgsCopy.m_heapType = pResource->GetAllocatorHeapType();

    // TODO: See if there's a good way to cache these guys.
    unique_comptr<Resource> renameResource = Resource::CreateResource(this, creationArgsCopy, threadingContext);
    renameResource->ZeroConstantBufferPadding();

    assert(renameResource->GetAllocatorHeapType() == AllocatorHeapType::Upload ||
        renameResource->GetAllocatorHeapType() == AllocatorHeapType::Decoder);

    assert(renameResource->GetAllocatorHeapType() == pResource->GetAllocatorHeapType());

    m_RenamesInFlight.GetLocked()->emplace_back(renameResource.get());
    return renameResource.get();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::Rename(Resource* pResource, Resource* pRenameResource)
{
    unique_comptr<Resource> renameResource(pRenameResource);

    Resource* rotate[2] = { pResource, renameResource.get() };
    RotateResourceIdentities(rotate, 2);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::RenameViaCopy(Resource* pResource, Resource* pRenameResource, UINT DirtyPlaneMask)
{
#ifdef USE_PIX
    PIXSetMarker(GetGraphicsCommandList(), 0ull, L"Rename resource via copy");
#endif
    unique_comptr<Resource> renameResource(pRenameResource);

    assert(pResource->AppDesc()->MipLevels() == 1 && pResource->AppDesc()->ArraySize() == 1);

    CDisablePredication DisablePredication(this);

    const UINT8 PlaneCount = (pResource->SubresourceMultiplier() * pResource->AppDesc()->NonOpaquePlaneCount());
    const bool EntireResourceDirty = (DirtyPlaneMask == (1u << PlaneCount) - 1u);
    if (EntireResourceDirty)
    {
        ResourceCopy(pResource, renameResource.get());
    }
    else
    {
        for (UINT subresource = 0; subresource < PlaneCount; ++subresource)
        {
            if (DirtyPlaneMask & (1 << subresource))
            {
                ResourceCopyRegion(pResource, subresource, 0, 0, 0, renameResource.get(), subresource, nullptr);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::DeleteRenameCookie(Resource* pRenameResource)
{
    auto LockedContainer = m_RenamesInFlight.GetLocked();
    auto iter = std::find_if(LockedContainer->begin(), LockedContainer->end(),
                             [pRenameResource](unique_comptr<Resource> const& r) { return r.get() == pRenameResource; });
    assert(iter != LockedContainer->end());

    // The only scenario where video is relevant here is for decode bitstream buffers. All other instances of
    // resource renaming are Map(DISCARD) graphics operations.
    COMMAND_LIST_TYPE CmdListType = pRenameResource->GetAllocatorHeapType() == AllocatorHeapType::Decoder ?
        COMMAND_LIST_TYPE::VIDEO_DECODE : COMMAND_LIST_TYPE::GRAPHICS;

    pRenameResource->UsedInCommandList(CmdListType, GetCommandListID(CmdListType));
    LockedContainer->erase(iter);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::MapDiscardBuffer(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMap )
{
#ifdef USE_PIX
    PIXSetMarker(0ull, L"Map(DISCARD) buffer");
#endif
    assert(pResource->NumSubresources() == 1 && pResource->GetEffectiveUsage() == RESOURCE_USAGE_DYNAMIC);
    assert(pResource->UnderlyingResourceIsSuballocated());

    bool bNeedRename = false;

    {
        auto& currentState = pResource->m_Identity->m_currentState;
        if (currentState.IsExclusiveState(Subresource))
        {
            bNeedRename = currentState.GetExclusiveSubresourceState(Subresource).FenceValue > 0;
        }
        else
        {
            auto& sharedState = currentState.GetSharedSubresourceState(Subresource);
            bNeedRename = std::any_of(std::begin(sharedState.FenceValues), std::end(sharedState.FenceValues),
                                      [](UINT64 Value) { return Value > 0; });
        }
    }

    if (bNeedRename)
    {
        if (!pResource->WaitForOutstandingResourcesIfNeeded(DoNotWait))
        {
            return false;
        }

        auto cookie = CreateRenameCookie(pResource, ResourceAllocationContext::ImmediateContextThreadLongLived);
        Rename(pResource, cookie);
        DeleteRenameCookie(cookie);
    }

    return MapUnderlying(pResource, Subresource, MapType, pReadWriteRange, pMap);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::MapDynamicTexture(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMap )
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Map of non-mappable resource");
#endif
    assert(pResource->GetEffectiveUsage() == RESOURCE_USAGE_DYNAMIC);
    assert(MapType != MAP_TYPE_WRITE_NOOVERWRITE);

    UINT MipIndex, PlaneIndex, ArrayIndex;
    pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

    const bool bNeedsReadbackCopy = MapType == MAP_TYPE_READ || MapType == MAP_TYPE_READWRITE
        // Note that MAP_TYPE_WRITE could be optimized to keep around a copy of the last-written
        // data and simply re-upload that on unmap, rather than doing a full read-modify-write loop
        // What we can't do, is skip the readback, because we don't have the current contents available
        // to be modified, and this isn't a discard operation.
        // Currently the only scenario that hits this is a shared vertex/index buffer in 9on12, so we don't care yet.
        || MapType == MAP_TYPE_WRITE;

    // If the app uses DoNotWait with a read flag, the translation layer is guaranteed to need to do GPU work
    // in order to copy the GPU data to a readback heap. As a result the first call to map will initiate the copy
    // and return that a draw is still in flight. The next map that's called after the copy is finished will
    // succeed.
    bool bReadbackCopyInFlight = pResource->GetCurrentCpuHeap(Subresource) != nullptr &&
        !pResource->GetDynamicTextureData(Subresource).AnyPlaneMapped();
    if (bReadbackCopyInFlight)
    {
        // If an app modifies the resource after a readback copy has been initiated but not mapped again,
        // make sure to invalidate the old copy so that the app 
        auto& exclusiveState = pResource->m_Identity->m_currentState.GetExclusiveSubresourceState(Subresource);
        const bool bPreviousCopyInvalid = 
            // If the app changed it's mind and decided it doesn't need to readback anymore,
            // we can throw out the copy since they'll be modifying the resource anyways
            !bNeedsReadbackCopy ||
            // The copy was done on the graphics queue so it's been modified
            // if the last write state was outside of graphics or newer than the
            // last exclusive state's fence value
            exclusiveState.CommandListType != COMMAND_LIST_TYPE::GRAPHICS ||
            exclusiveState.FenceValue > pResource->GetLastCopyCommandListID(Subresource);

        if (bPreviousCopyInvalid)
        {
            pResource->SetCurrentCpuHeap(Subresource, nullptr);
            bReadbackCopyInFlight = false;
        }
    }

    // For planar textures, the upload buffer is created for all planes when all planes
    // were previously not mapped
    if (!pResource->GetDynamicTextureData(Subresource).AnyPlaneMapped())
    {
        if (!bReadbackCopyInFlight)
        {
            assert(pResource->GetEffectiveUsage() == RESOURCE_USAGE_DYNAMIC);

            auto desc12 = pResource->m_creationArgs.m_desc12;

            auto& Placement = pResource->GetSubresourcePlacement(Subresource);
            desc12.MipLevels = 1;
            desc12.Width = Placement.Footprint.Width;
            desc12.Height = Placement.Footprint.Height;
            desc12.DepthOrArraySize = static_cast<UINT16>(
                desc12.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ?
                    Placement.Footprint.Depth : 1);

            RESOURCE_CPU_ACCESS cpuAccess =   (bNeedsReadbackCopy ? RESOURCE_CPU_ACCESS_READ : RESOURCE_CPU_ACCESS_NONE) 
                                            | (MapType != MAP_TYPE_READ ? RESOURCE_CPU_ACCESS_WRITE : RESOURCE_CPU_ACCESS_NONE);

            auto creationArgsCopy = pResource->m_creationArgs;
            creationArgsCopy.m_appDesc = AppResourceDesc(desc12, RESOURCE_USAGE_STAGING, cpuAccess, RESOURCE_BIND_NONE);

            UINT64 resourceSize = 0;
            m_pDevice12->GetCopyableFootprints(&desc12, 0, creationArgsCopy.m_appDesc.NonOpaquePlaneCount(), 0, nullptr, nullptr, nullptr, &resourceSize);

            creationArgsCopy.m_heapDesc = CD3DX12_HEAP_DESC(resourceSize, Resource::GetD3D12HeapType(RESOURCE_USAGE_STAGING, cpuAccess));
            creationArgsCopy.m_heapType = AllocatorHeapType::None;
            creationArgsCopy.m_flags11.BindFlags = 0;
            creationArgsCopy.m_flags11.MiscFlags = 0;
            creationArgsCopy.m_flags11.CPUAccessFlags = (bNeedsReadbackCopy ? D3D11_CPU_ACCESS_READ : 0) 
                                            | (MapType != MAP_TYPE_READ ? D3D11_CPU_ACCESS_WRITE : 0);
            creationArgsCopy.m_flags11.StructureByteStride = 0;

            unique_comptr<Resource> renameResource = Resource::CreateResource(this, creationArgsCopy, ResourceAllocationContext::FreeThread);
            pResource->SetCurrentCpuHeap(Subresource, renameResource.get());
            
            CD3DX12_RANGE ReadRange(0, 0); // Map(DISCARD) is write-only
            D3D12_RANGE MappedRange = renameResource->GetSubresourceRange(0, pReadWriteRange);

            if (bNeedsReadbackCopy)
            {
                // Maintain the illusion that this data is read by the CPU directly from the mapped resource.
                CDisablePredication DisablePredication(this);

                UINT DstX = pReadWriteRange ? pReadWriteRange->left : 0u;
                UINT DstY = pReadWriteRange ? pReadWriteRange->top : 0u;
                UINT DstZ = pReadWriteRange ? pReadWriteRange->front : 0u;

                // Copy each plane.
                for (UINT iPlane = 0; iPlane < pResource->AppDesc()->NonOpaquePlaneCount(); ++iPlane)
                {
                    UINT planeSubresource = pResource->GetSubresourceIndex(iPlane, MipIndex, ArrayIndex);
                    ResourceCopyRegion(renameResource.get(), iPlane, DstX, DstY, DstZ, pResource, planeSubresource, pReadWriteRange);
                }

                pResource->SetLastCopyCommandListID(Subresource, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
            }
        }
    }

    {
        // Synchronize for the Map.  Renamed resource has no mips or array slices.
        Resource* pRenameResource = pResource->GetCurrentCpuHeap(Subresource);
        assert(pRenameResource->AppDesc()->MipLevels() == 1);
        assert(pRenameResource->AppDesc()->ArraySize() == 1);

        if (!MapUnderlyingSynchronize(pRenameResource, PlaneIndex, MapType, DoNotWait, pReadWriteRange, pMap))
        {
            return false;
        }
    }

    // Record that the given plane was mapped and is now dirty
    pResource->GetDynamicTextureData(Subresource).m_MappedPlaneRefCount[PlaneIndex]++;
    pResource->GetDynamicTextureData(Subresource).m_DirtyPlaneMask |= (1 << PlaneIndex);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::MapUnderlying(Resource* pResource, UINT Subresource, MAP_TYPE MapType, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMap )
{
    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC || pResource->AppDesc()->Usage() == RESOURCE_USAGE_STAGING);
    assert(pResource->OwnsReadbackHeap() || pResource->UnderlyingResourceIsSuballocated());

    auto pResource12 = pResource->GetUnderlyingResource();
    void* pData;

    // Write-only means read range can be empty
    D3D12_RANGE MappedRange = pResource->GetSubresourceRange(Subresource, pReadWriteRange);
    D3D12_RANGE ReadRange =
        MapType == MAP_TYPE_WRITE ? CD3DX12_RANGE(0, 0) : MappedRange;
    HRESULT hr = pResource12->Map(0, &ReadRange, &pData);
    ThrowFailure(hr); // throw( _com_error )

    auto& SubresourceInfo = pResource->GetSubresourcePlacement(Subresource);
    pMap->pData = reinterpret_cast<void*>(reinterpret_cast<size_t>(pData) + MappedRange.Begin);
    pMap->RowPitch = SubresourceInfo.Footprint.RowPitch;
    pMap->DepthPitch = pResource->DepthPitch(Subresource);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::SynchronizeForMap(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait)
{
    if (MapType == MAP_TYPE_READ || MapType == MAP_TYPE_READWRITE)
    {
        GetCommandListManager(COMMAND_LIST_TYPE::GRAPHICS)->ReadbackInitiated();
    }

    auto& CurrentState = pResource->m_Identity->m_currentState;
    assert(CurrentState.SupportsSimultaneousAccess() ||
           // Disabling simultaneous access for suballocated buffers but they're technically okay to map this way
           !pResource->GetIdentity()->m_bOwnsUnderlyingResource ||
           // 9on12 special case
           pResource->OwnsReadbackHeap() ||
           // For Map(DEFAULT) we should've made sure we're in common
           CurrentState.GetExclusiveSubresourceState(Subresource).State == D3D12_RESOURCE_STATE_COMMON ||
           // Or we're not mapping the actual resource
           pResource->GetCurrentCpuHeap(Subresource) != nullptr);

    // We want to synchronize against the last command list to write to this subresource if
    // either the last op to it was a write (or in the same command list as a write),
    // or if we're only mapping it for read.
    bool bUseExclusiveState = MapType == MAP_TYPE_READ || CurrentState.IsExclusiveState(Subresource);
    if (bUseExclusiveState)
    {
        auto& ExclusiveState = CurrentState.GetExclusiveSubresourceState(Subresource);
        if (ExclusiveState.CommandListType == COMMAND_LIST_TYPE::UNKNOWN)
        {
            // Resource either has never been used, or at least never written to.
            return true;
        }
        return WaitForFenceValue(ExclusiveState.CommandListType, ExclusiveState.FenceValue, DoNotWait); // throws
    }
    else
    {
        // Wait for all reads of the resource to be done before allowing write access.
        auto& SharedState = CurrentState.GetSharedSubresourceState(Subresource);
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            if (SharedState.FenceValues[i] > 0 &&
                !WaitForFenceValue((COMMAND_LIST_TYPE)i, SharedState.FenceValues[i], DoNotWait)) // throws
            {
                return false;
            }
        }
        return true;
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::WaitForFenceValue(COMMAND_LIST_TYPE type, UINT64 FenceValue, bool DoNotWait)
{
    if (DoNotWait)
    {
        if (FenceValue == GetCommandListID(type))
        {
            SubmitCommandList(type); // throws on CommandListManager::CloseCommandList(...)
        }
        if (FenceValue > GetCompletedFenceValue(type))
        {
            return false;
        }
        return true;
    }
    else
    {
        return WaitForFenceValue(type, FenceValue); // throws
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::Map(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMappedSubresource)
{
    switch (pResource->AppDesc()->Usage())
    {
    case RESOURCE_USAGE_DEFAULT:
        return MapDefault(pResource, Subresource, MapType, false, pReadWriteRange, pMappedSubresource);
    case RESOURCE_USAGE_DYNAMIC:
        switch (MapType)
        {
            case MAP_TYPE_READ:
            case MAP_TYPE_READWRITE:
                if (pResource->m_creationArgs.m_heapDesc.Properties.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
                {
                    return MapUnderlyingSynchronize(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
                else
                {
                    return MapDynamicTexture(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
            case MAP_TYPE_WRITE_DISCARD:
                if (pResource->m_creationArgs.m_heapDesc.Properties.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
                {
                    return MapDiscardBuffer(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
                else
                {
                    return MapDynamicTexture(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
            case MAP_TYPE_WRITE_NOOVERWRITE:
                assert(pResource->AppDesc()->CPUAccessFlags() == RESOURCE_CPU_ACCESS_WRITE);
                assert(pResource->AppDesc()->ResourceDimension() == D3D12_RESOURCE_DIMENSION_BUFFER);
                return MapUnderlying(pResource, Subresource, MapType, pReadWriteRange, pMappedSubresource);
            case MAP_TYPE_WRITE:
                assert(pResource->AppDesc()->ResourceDimension() == D3D12_RESOURCE_DIMENSION_BUFFER);
                if (pResource->m_creationArgs.m_heapDesc.Properties.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
                {
                    return MapUnderlyingSynchronize(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
                else
                {
                    return MapDynamicTexture(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
                }
        }
        break;
    case RESOURCE_USAGE_STAGING:
        return MapUnderlyingSynchronize(pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
    case RESOURCE_USAGE_IMMUTABLE:
    default:
        assert(false);
    }
    return false;
}

void TRANSLATION_API ImmediateContext::Unmap(Resource* pResource, UINT Subresource, MAP_TYPE MapType, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    switch (pResource->AppDesc()->Usage())
    {
    case RESOURCE_USAGE_DEFAULT:
        UnmapDefault(pResource, Subresource, pReadWriteRange);
        break;
    case RESOURCE_USAGE_DYNAMIC:
        if (pResource->m_creationArgs.m_heapDesc.Properties.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
        {
            UnmapUnderlyingSimple(pResource, Subresource, pReadWriteRange);
        }
        else
        {
            UnmapDynamicTexture(pResource, Subresource, pReadWriteRange, MapType != MAP_TYPE_READ);
        }
        break;
    case RESOURCE_USAGE_STAGING:
        UnmapUnderlyingStaging(pResource, Subresource, pReadWriteRange);
        break;
    case RESOURCE_USAGE_IMMUTABLE:
        assert(false);
        break;
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::MapDefault(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMap )
{
    auto pResource12 = pResource->GetUnderlyingResource();

    if (pResource->Parent()->ResourceDimension12() != D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        m_ResourceStateManager.TransitionSubresource(pResource, Subresource, D3D12_RESOURCE_STATE_COMMON, COMMAND_LIST_TYPE::UNKNOWN, SubresourceTransitionFlags::StateMatchExact | SubresourceTransitionFlags::NotUsedInCommandListIfNoStateChange);
        m_ResourceStateManager.ApplyAllResourceTransitions();
        if (MapType == MAP_TYPE_WRITE_NOOVERWRITE)
        {
            MapType = MAP_TYPE_WRITE;
        }
    }

    // Write-only means read range can be empty
    bool bWriteOnly = (MapType == MAP_TYPE_WRITE || MapType == MAP_TYPE_WRITE_NOOVERWRITE);
    D3D12_RANGE MappedRange = pResource->GetSubresourceRange(Subresource, pReadWriteRange);
    D3D12_RANGE ReadRange = bWriteOnly ? CD3DX12_RANGE(0, 0) : MappedRange;
    // If we know we are not reading, pass an empty range, otherwise pass a null (full) range
    D3D12_RANGE* pNonStandardReadRange = bWriteOnly ? &ReadRange : nullptr;

    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_DEFAULT);
    bool bSynchronizationSucceeded = true;
    bool bSyncronizationNeeded = MapType != MAP_TYPE_WRITE_NOOVERWRITE;
    if (bSyncronizationNeeded)
    {
        bSynchronizationSucceeded = SynchronizeForMap(pResource, Subresource, MapType, DoNotWait);
    }

    if (bSynchronizationSucceeded)
    {
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &Placement = pResource->GetSubresourcePlacement(Subresource);
        if (pResource->m_Identity->m_bPlacedTexture ||
            pResource->Parent()->ResourceDimension12() == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            // Map default row-major texture or buffer
            pResource12->Map(0, &ReadRange, &pMap->pData);

            pMap->pData = reinterpret_cast<BYTE *>(pMap->pData) + MappedRange.Begin;
            pMap->RowPitch = Placement.Footprint.RowPitch;
            pMap->DepthPitch = pResource->DepthPitch(Subresource);
        }
        else if (pResource->Parent()->ApiTextureLayout12() == D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE)
        {
            // Not supporting map calls on standard swizzle textures with a specified subrange
            assert(!pReadWriteRange);

            // Map default standard swizzle texture
            D3D11_TILE_SHAPE TileShape;
            CD3D11FormatHelper::GetTileShape(&TileShape, Placement.Footprint.Format,
                                             pResource->Parent()->ResourceDimension11(), pResource->AppDesc()->Samples());
            pResource12->Map(Subresource, pNonStandardReadRange, &pMap->pData);

            // Logic borrowed from WARP
            UINT TileWidth = (Placement.Footprint.Width + TileShape.WidthInTexels - 1) / TileShape.WidthInTexels;
            UINT TileHeight = (Placement.Footprint.Height + TileShape.HeightInTexels - 1) / TileShape.HeightInTexels;
            pMap->RowPitch = TileWidth * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
            pMap->DepthPitch = TileWidth * TileHeight * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
        }
        else
        {
            // Opaque: Simply cache the map
            pResource12->Map(Subresource, pNonStandardReadRange, nullptr);
        }
    }
    return bSynchronizationSucceeded;
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::WriteToSubresource(Resource* pDstResource, UINT DstSubresource, _In_opt_ const D3D11_BOX* pDstBox, 
    const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
    pDstResource->GetUnderlyingResource()->WriteToSubresource(DstSubresource, reinterpret_cast<const D3D12_BOX*>(pDstBox), pSrcData, SrcRowPitch, SrcDepthPitch);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ReadFromSubresource(void* pDstData, UINT DstRowPitch, UINT DstDepthPitch,
    Resource* pSrcResource, UINT SrcSubresource, _In_opt_ const D3D11_BOX* pSrcBox)
{
    pSrcResource->GetUnderlyingResource()->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource, reinterpret_cast<const D3D12_BOX*>(pSrcBox));
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API ImmediateContext::MapUnderlyingSynchronize(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMap )
{
    bool bSynchronizeSucceeded = SynchronizeForMap(pResource, Subresource, MapType, DoNotWait);
    if (bSynchronizeSucceeded)
    {
        if (pResource->m_FormatEmulationStagingData.empty())
        {
            MapUnderlying(pResource, Subresource, MapType, pReadWriteRange, pMap);
        }
        else
        {
            assert(!pResource->m_Identity->m_bOwnsUnderlyingResource);

            Resource::EmulatedFormatMapState State = (MapType == MAP_TYPE_READ) ? Resource::EmulatedFormatMapState::Read :
                (MapType == MAP_TYPE_READWRITE) ? Resource::EmulatedFormatMapState::ReadWrite : Resource::EmulatedFormatMapState::Write;

            ZeroMemory(pMap, sizeof(*pMap));
            SIZE_T StagingBufferSize = 0;
            CD3DX12_RANGE WrittenRange(0, 0);
            void* pMapped = nullptr;

            if (pResource->m_FormatEmulationStagingData[Subresource].m_MapState == State)
            {
                ++pResource->m_FormatEmulationStagingData[Subresource].m_MapRefCount;                

                if (pResource->GetFormatEmulation() == FormatEmulation::YV12)
                {
                    MapUnderlying(pResource, Subresource, MapType, pReadWriteRange, pMap);
                }
                else
                {
                    assert(pResource->GetFormatEmulation() == FormatEmulation::None);
                    pMap->pData = pResource->GetFormatEmulationSubresourceStagingAllocation(Subresource).get();
                }
            }
            else if (pResource->m_FormatEmulationStagingData[Subresource].m_MapState == Resource::EmulatedFormatMapState::None)
            {
                if (pResource->GetFormatEmulation() == FormatEmulation::YV12)
                {
                    assert(pResource->AppDesc()->Format() == DXGI_FORMAT_NV12);

                    MapUnderlying(pResource, Subresource, MapType, pReadWriteRange, pMap);

                    UINT MipIndex, PlaneIndex, ArrayIndex;
                    pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

                    if (PlaneIndex == 1)
                    {
                        auto& UVPlacement = pResource->GetSubresourcePlacement(pResource->GetSubresourceIndex(1, MipIndex, ArrayIndex));
                        UINT YV12RowPitch = UVPlacement.Footprint.RowPitch >> 1;
                        UINT UVWidth = UVPlacement.Footprint.Width;
                        UINT UVHeight =  UVPlacement.Footprint.Height;

                        BYTE* pData = static_cast<BYTE*>(pMap->pData);                        
                        auto pbUTemp = m_SyncronousOpScrachSpace.GetBuffer(UVWidth * UVHeight);

                        for (UINT i = 0; i < UVPlacement.Footprint.Height; ++i)
                        {
                            BYTE* pSrcUV = pData + i * pMap->RowPitch;
                            BYTE* pDstV = pData + i * YV12RowPitch;
                            BYTE* pDstU = pbUTemp + i * UVWidth;

                            for (UINT j = 0; j < UVWidth; ++j)
                            {
                                *pDstU++ = *pSrcUV++;
                                *pDstV++ = *pSrcUV++;
                            }
                        }

                        for (UINT i = 0; i < UVHeight; ++i)
                        {
                            BYTE* pDstU = pData + (UVHeight + i) * YV12RowPitch;
                            BYTE* pSrcU = pbUTemp + i * UVWidth;

                            memcpy (pDstU, pSrcU, UVWidth);
                        }
                    }
                }
                else
                {
                    assert(!pReadWriteRange); // TODO: Add handling for DSVs that are only getting a subsection mapped
                    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_STAGING);
                    assert(pResource->SubresourceMultiplier() == 2);
                    assert(pResource->GetFormatEmulation() == FormatEmulation::None);

                    {
                        auto& Placement = pResource->GetSubresourcePlacement(pResource->GetExtendedSubresourceIndex(Subresource, 0));
                        CD3D11FormatHelper::CalculateResourceSize(Placement.Footprint.Width,
                                                                    Placement.Footprint.Height,
                                                                    1,
                                                                    pResource->AppDesc()->Format(),
                                                                    1,
                                                                    1,
                                                                    StagingBufferSize,
                                                                    reinterpret_cast<D3D11_MAPPED_SUBRESOURCE*>(pMap));
                    }

                    auto& pStagingBuffer = pResource->GetFormatEmulationSubresourceStagingAllocation(Subresource);

                    if (!pStagingBuffer)
                    {
                        void* pData = AlignedHeapAlloc16(StagingBufferSize);
                        if (!pData)
                        {
                            ThrowFailure(E_OUTOFMEMORY);
                        }
                        pStagingBuffer.reset(reinterpret_cast<BYTE*>(pData));
                    }

                    // The interleaving copy uses |= to write each plane's contents into the dest,
                    // which will break if the destination contains garbage
                    ZeroMemory(pStagingBuffer.get(), StagingBufferSize);

                    // Note: Always have to interleave the source contents
                    // Since MAP_WRITE does not imply old contents are discarded, we must provide
                    // the buffer filled with the correct previous contents
                    CD3DX12_RANGE ReadRange(pResource->GetSubresourceRange(pResource->GetExtendedSubresourceIndex(Subresource, 0)).Begin,
                                            pResource->GetSubresourceRange(pResource->GetExtendedSubresourceIndex(Subresource, 1)).End);

                    pResource->GetUnderlyingResource()->Map(0, &ReadRange, &pMapped);

                    for (UINT i = 0; i < pResource->SubresourceMultiplier(); ++i)
                    {
                        UINT ExtendedSubresource = pResource->GetExtendedSubresourceIndex(Subresource, i);
                        auto& Placement = pResource->GetSubresourcePlacement(ExtendedSubresource);

                        BYTE* pSrcPlane = reinterpret_cast<BYTE*>(pMapped) + Placement.Offset;

                        DXGI_FORMAT parentFormat = GetParentForFormat(pResource->AppDesc()->Format());
                        DepthStencilInterleavingReadback(parentFormat, i,
                                                            pSrcPlane, Placement.Footprint.RowPitch,
                                                            pStagingBuffer.get(), pMap->RowPitch,
                                                            Placement.Footprint.Width, Placement.Footprint.Height);
                        
                    }

                    pResource->GetUnderlyingResource()->Unmap(0, &WrittenRange);
                    pMap->pData = pStagingBuffer.get();
                }

                pResource->m_FormatEmulationStagingData[Subresource].m_MapState = State;
                pResource->m_FormatEmulationStagingData[Subresource].m_MapRefCount = 1;
            }
            else
            {
                ThrowFailure(E_FAIL);
            }
        }
    }
    return bSynchronizeSucceeded;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UnmapDefault(Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    auto pResource12 = pResource->GetUnderlyingResource();

    if (pResource->m_Identity->m_bPlacedTexture)
    {
        Subresource = 0;
    }

    // No way to tell whether the map that is being undone was a READ, WRITE, or both, so key off CPU access flags
    // to determine if data could've been written by the CPU
    bool bCouldBeWritten = (pResource->AppDesc()->CPUAccessFlags() & RESOURCE_CPU_ACCESS_WRITE) != 0;
    bool bRowMajorPattern = pResource->m_Identity->m_bPlacedTexture ||
                            pResource->Parent()->ResourceDimension12() == D3D12_RESOURCE_DIMENSION_BUFFER;
    // If we couldn't have written anything, pass an empty range
    D3D12_RANGE WrittenRange = bCouldBeWritten ?
        pResource->GetSubresourceRange(Subresource, pReadWriteRange) : CD3DX12_RANGE(0, 0);
    // If we know how much we could've written, pass the range. If we know we didn't, pass an empty range. Otherwise, pass null.
    D3D12_RANGE* pWrittenRange = bRowMajorPattern || !bCouldBeWritten ?
        &WrittenRange : nullptr;
    pResource12->Unmap(Subresource, pWrittenRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UnmapUnderlyingSimple(Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC || pResource->AppDesc()->Usage() == RESOURCE_USAGE_STAGING);
    assert(pResource->OwnsReadbackHeap() || pResource->UnderlyingResourceIsSuballocated());
    
    auto pResource12 = pResource->GetUnderlyingResource();
    D3D12_RANGE WrittenRange = pResource->GetSubresourceRange(Subresource, pReadWriteRange);
    pResource12->Unmap(0, &WrittenRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UnmapUnderlyingStaging(Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC || pResource->AppDesc()->Usage() == RESOURCE_USAGE_STAGING);
    assert(pResource->OwnsReadbackHeap() ||  !pResource->m_Identity->m_bOwnsUnderlyingResource);

    if (pResource->m_FormatEmulationStagingData.empty())
    {
        auto pResource12 = pResource->GetUnderlyingResource();        

        // No way to tell whether the map that is being undone was a READ, WRITE, or both, so key off CPU access flags
        // to determine if data could've been written by the CPU. If we couldn't have written anything, pass an empty range
        bool bCouldBeWritten = (pResource->AppDesc()->CPUAccessFlags() & RESOURCE_CPU_ACCESS_WRITE) != 0;
        D3D12_RANGE WrittenRange = bCouldBeWritten ?
            pResource->GetSubresourceRange(Subresource, pReadWriteRange) : CD3DX12_RANGE(0, 0);

        pResource12->Unmap(0, &WrittenRange);
    }
    else if (pResource->m_FormatEmulationStagingData[Subresource].m_MapState == Resource::EmulatedFormatMapState::None)
    {
        ThrowFailure(E_FAIL);
    }
    else if (--pResource->m_FormatEmulationStagingData[Subresource].m_MapRefCount == 0)
    {
        assert(!pReadWriteRange); // Not supporting mapped DSVs with subranges
        auto State = pResource->m_FormatEmulationStagingData[Subresource].m_MapState;
        bool bDoCopy = (State != Resource::EmulatedFormatMapState::Read) 
                    || pResource->IsInplaceFormatEmulation();

        if (bDoCopy)
        {
            CD3DX12_RANGE ReadRange(0, 0);
            void* pMapped;

            if (pResource->GetFormatEmulation() == FormatEmulation::YV12)
            {
                UINT MipIndex, PlaneIndex, ArrayIndex;
                pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

                if (PlaneIndex == 1)
                {
                    auto& UVPlacement = pResource->GetSubresourcePlacement(Subresource);
                    UINT YV12RowPitch = UVPlacement.Footprint.RowPitch >> 1;
                    UINT UVWidth = UVPlacement.Footprint.Width;
                    UINT UVHeight =  UVPlacement.Footprint.Height;
                    UINT UVRowPitch = UVPlacement.Footprint.RowPitch;

                    pResource->GetUnderlyingResource()->Map(0, &ReadRange, &pMapped);

                    BYTE* pData = static_cast<BYTE*>(pMapped) + UVPlacement.Offset;
                    BYTE* pbVTemp = m_SyncronousOpScrachSpace.GetBuffer(UVWidth * UVHeight);

                    for (UINT i = 0; i < UVHeight; ++i)
                    {
                        BYTE* pDstV = pbVTemp + i * UVWidth;
                        BYTE* pSrcV = pData + i * YV12RowPitch;
                        memcpy (pDstV, pSrcV, UVWidth);
                    }

                    for (UINT i = 0; i < UVHeight; ++i)
                    {
                        BYTE* pSrcV = pbVTemp + i * UVWidth; 
                        BYTE* pSrcU = pData + (UVHeight + i ) * YV12RowPitch;
                        BYTE* pDstUV = pData + i * UVRowPitch;
                        

                        for (UINT j = 0; j < UVWidth; ++j)
                        {
                            *pDstUV++ = *pSrcU++;
                            *pDstUV++ = *pSrcV++;
                        }
                    }

                    pResource->GetUnderlyingResource()->Unmap(0, &pResource->GetSubresourceRange(Subresource));
                }

                pResource->GetUnderlyingResource()->Unmap(0, &pResource->GetSubresourceRange(Subresource));
            }
            else
            {
                auto& pStagingBuffer = pResource->GetFormatEmulationSubresourceStagingAllocation(Subresource);

                assert(pStagingBuffer.get() != nullptr);
                assert(pResource->GetFormatEmulation() == FormatEmulation::None);

                UINT InterleavedRowPitch;
                {
                    auto& Placement = pResource->GetSubresourcePlacement(pResource->GetExtendedSubresourceIndex(Subresource, 0));
                    CD3D11FormatHelper::CalculateMinimumRowMajorRowPitch(pResource->AppDesc()->Format(), Placement.Footprint.Width, InterleavedRowPitch);
                }

                CD3DX12_RANGE WrittenRange(pResource->GetSubresourceRange(pResource->GetExtendedSubresourceIndex(Subresource, 0)).Begin,
                                           pResource->GetSubresourceRange(pResource->GetExtendedSubresourceIndex(Subresource, 1)).End);

                pResource->GetUnderlyingResource()->Map(0, &ReadRange, &pMapped);

                for (UINT i = 0; i < pResource->SubresourceMultiplier(); ++i)
                {
                    UINT ExtendedSubresource = pResource->GetExtendedSubresourceIndex(Subresource, i);
                    auto& Placement = pResource->GetSubresourcePlacement(ExtendedSubresource);
    
                    BYTE* pDstPlane = reinterpret_cast<BYTE*>(pMapped) + Placement.Offset;
    
                    DXGI_FORMAT parentFormat = GetParentForFormat(pResource->AppDesc()->Format());
                    DepthStencilDeInterleavingUpload(parentFormat, i,
                                                     pStagingBuffer.get(), InterleavedRowPitch,
                                                     pDstPlane, Placement.Footprint.RowPitch,
                                                     Placement.Footprint.Width, Placement.Footprint.Height);
                }
    
                pResource->GetUnderlyingResource()->Unmap(0, &WrittenRange);
                pStagingBuffer.reset(nullptr);
            }
        }
        else if (pResource->GetFormatEmulation() != FormatEmulation::None)
        {
            assert(pResource->GetFormatEmulation() == FormatEmulation::YV12);
            pResource->GetUnderlyingResource()->Unmap(0, &CD3DX12_RANGE(0, 0));
        }
        pResource->m_FormatEmulationStagingData[Subresource].m_MapState = Resource::EmulatedFormatMapState::None;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::UnmapDynamicTexture(Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange, bool bUploadMappedContents)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"Unmap non-mappable resource");
#endif
    UINT MipIndex, PlaneIndex, ArrayIndex;
    pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

    assert(pResource->AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC);
    assert(pResource->GetCurrentCpuHeap(Subresource) != nullptr);

    Resource* pRenameResource = pResource->GetCurrentCpuHeap(Subresource);

    // If multiple planes of the dynamic texture were mapped simultaneously, only copy
    // data from the upload buffer once all planes have been unmapped.
    assert(pResource->GetDynamicTextureData(Subresource).m_MappedPlaneRefCount[PlaneIndex] > 0);
    pResource->GetDynamicTextureData(Subresource).m_MappedPlaneRefCount[PlaneIndex]--;
    if (bUploadMappedContents)
    {
        UnmapUnderlyingStaging(pRenameResource, PlaneIndex, pReadWriteRange);
    }
    else
    {
        UnmapUnderlyingSimple(pRenameResource, PlaneIndex, pReadWriteRange);
    }

    if (pResource->GetDynamicTextureData(Subresource).AnyPlaneMapped())
    {
        return;
    }

    if(bUploadMappedContents)
    {
        // Maintain the illusion that data is written by the CPU directly to this resource.
        CDisablePredication DisablePredication(this);

        UINT DstX = pReadWriteRange ? pReadWriteRange->left : 0u;
        UINT DstY = pReadWriteRange ? pReadWriteRange->top : 0u;
        UINT DstZ = pReadWriteRange ? pReadWriteRange->front : 0u;

        // Copy each plane.
        for (UINT iPlane = 0; iPlane < pResource->AppDesc()->NonOpaquePlaneCount(); ++iPlane)
        {
            if (   pResource->Parent()->ResourceDimension12() == D3D12_RESOURCE_DIMENSION_BUFFER
                || pResource->GetDynamicTextureData(Subresource).m_DirtyPlaneMask & (1 << iPlane))
            {
                UINT planeSubresource = pResource->GetSubresourceIndex(iPlane, MipIndex, ArrayIndex);
                ResourceCopyRegion(pResource, planeSubresource, DstX, DstY, DstZ, pRenameResource, iPlane, pReadWriteRange);
            }
        }

        pResource->GetDynamicTextureData(Subresource).m_DirtyPlaneMask = 0;
    }

    pResource->SetCurrentCpuHeap(Subresource, nullptr);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::GetMipPacking(Resource* pResource, _Out_ UINT* pNumPackedMips, _Out_ UINT* pNumTilesForPackedMips )
{
    *pNumPackedMips = pResource->AppDesc()->MipLevels() - pResource->m_TiledResource.m_NumStandardMips;
    *pNumTilesForPackedMips = pResource->m_TiledResource.m_NumTilesForPackedMips;
}

//----------------------------------------------------------------------------------------------------------------------------------
HRESULT TRANSLATION_API ImmediateContext::CheckFormatSupport(_Out_ D3D12_FEATURE_DATA_FORMAT_SUPPORT& formatData)
{
    return m_pDevice12->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatData, sizeof(formatData));
}

//----------------------------------------------------------------------------------------------------------------------------------
bool ImmediateContext::SupportsRenderTarget(DXGI_FORMAT Format)
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT SupportStruct = {};
    SupportStruct.Format = Format;

    return (   SUCCEEDED(CheckFormatSupport(SupportStruct))
            && (SupportStruct.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) == D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CheckMultisampleQualityLevels(DXGI_FORMAT format, UINT SampleCount, D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS Flags, _Out_ UINT* pNumQualityLevels )
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS SupportStruct;
    SupportStruct.Format = format;
    SupportStruct.SampleCount = SampleCount;
    SupportStruct.Flags = Flags;
    HRESULT hr = m_pDevice12->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &SupportStruct, sizeof(SupportStruct));

    *pNumQualityLevels = SUCCEEDED(hr) ? SupportStruct.NumQualityLevels : 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CheckFeatureSupport(D3D12_FEATURE Feature, _Inout_updates_bytes_(FeatureSupportDataSize)void* pFeatureSupportData, UINT FeatureSupportDataSize)
{
    ThrowFailure(m_pDevice12->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize));
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::CDisablePredication::CDisablePredication(ImmediateContext* pParent)
    : m_pParent(pParent)
{
    if (m_pParent)
    {
        m_pParent->SetPredicationInternal(nullptr, false);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::CDisablePredication::~CDisablePredication()
{
    // Restore the predicate
    if (m_pParent && m_pParent->m_CurrentState.m_pPredicate)
    {
        m_pParent->m_CurrentState.m_pPredicate->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, m_pParent->GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

        m_pParent->SetPredicationInternal(m_pParent->m_CurrentState.m_pPredicate, m_pParent->m_PredicateValue);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::CopyDataToBuffer(
    ID3D12Resource* pDstResource,
    UINT DstOffset,
    const void* pData,
    UINT Size
    ) noexcept(false)
{
    // this operation should not be predicated (even if the application has enabled predication at the D3D11 API)
    CDisablePredication DisablePredication(this);

    const UINT AlignedSize = 1024; // To ensure good pool re-use
    assert(Size <= AlignedSize);

    auto UploadHeap = AcquireSuballocatedHeap(AllocatorHeapType::Upload, AlignedSize, ResourceAllocationContext::ImmediateContextThreadTemporary); // throw( _com_error )

    void* pMapped;
    CD3DX12_RANGE ReadRange(0, 0);
    HRESULT hr = UploadHeap.Map(0, &ReadRange, &pMapped);
    ThrowFailure(hr); // throw( _com_error )

    memcpy(pMapped, pData, Size);

    CD3DX12_RANGE WrittenRange(0, Size);
    UploadHeap.Unmap(0, &WrittenRange);

    GetGraphicsCommandList()->CopyBufferRegion(
        pDstResource,
        DstOffset,
        UploadHeap.GetResource(),
        UploadHeap.GetOffset(),
        Size
        );

    AdditionalCommandsAdded(COMMAND_LIST_TYPE::GRAPHICS);
    ReleaseSuballocatedHeap(
        AllocatorHeapType::Upload, 
        UploadHeap,
        GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS),
        COMMAND_LIST_TYPE::GRAPHICS);
        
    PostUpload();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetHardwareProtection(Resource*, INT)
{
    assert(false);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetHardwareProtectionState(BOOL)
{
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::RotateResourceIdentities(Resource* const* ppResources, UINT Resources)
{
#ifdef USE_PIX
    PIXSetMarker(0ull, L"Swap resource identities");
#endif
    Resource* pLastResource = ppResources[0];
    for (UINT i = 1; i <= Resources; ++i)
    {
        ++pLastResource->m_AllUniqueness;
        ++pLastResource->m_SRVUniqueness;
        CResourceBindings& bindingState = pLastResource->m_currentBindings;

        // Set dirty bits for all bound SRVs of this resource
        auto pHead = &bindingState.m_ShaderResourceViewList;
        for (auto pCur = pHead->Flink; pCur != pHead; pCur = pCur->Flink)
        {
            auto& viewBindings = *CONTAINING_RECORD(pCur, CViewBindings<ShaderResourceViewType>, m_ViewBindingList);
            for (UINT stage = 0; stage < ShaderStageCount; ++stage)
            {
                auto& stageState = m_CurrentState.GetStageState((EShaderStage)stage);
                stageState.m_SRVs.SetDirtyBits(viewBindings.m_BindPoints[stage]);
            }
        }

        // Set dirty bits for all bound UAVs of this resource
        pHead = &bindingState.m_UnorderedAccessViewList;
        for (auto pCur = pHead->Flink; pCur != pHead; pCur = pCur->Flink)
        {
            auto& viewBindings = *CONTAINING_RECORD(pCur, CViewBindings<UnorderedAccessViewType>, m_ViewBindingList);
            m_CurrentState.m_UAVs.SetDirtyBits(viewBindings.m_BindPoints[e_Graphics]);
            m_CurrentState.m_CSUAVs.SetDirtyBits(viewBindings.m_BindPoints[e_Compute]);
        }

        // If the resource is bound as a render target, set the RTV dirty bit
        if (bindingState.IsBoundAsRenderTarget())
        {
            m_DirtyStates |= e_RenderTargetsDirty;
        }

        // Handle buffer rotation too to simplify rename operations.
        if (bindingState.IsBoundAsVertexBuffer())
        {
            m_DirtyStates |= e_VertexBuffersDirty;
        }
        if (bindingState.IsBoundAsIndexBuffer())
        {
            m_DirtyStates |= e_IndexBufferDirty;
        }
        for (UINT stage = 0; stage < ShaderStageCount; ++stage)
        {
            auto& stageState = m_CurrentState.GetStageState((EShaderStage)stage);
            stageState.m_CBs.SetDirtyBits(bindingState.m_ConstantBufferBindings[stage]);
        }

        m_ResourceStateManager.TransitionResourceForBindings(pLastResource);

        if (i < Resources)
        {
            Resource* pCurrentResource = ppResources[i];
            pCurrentResource->SwapIdentities(*pLastResource);
            pLastResource = pCurrentResource;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::TransitionResourceForView(ViewBase* pView, D3D12_RESOURCE_STATES desiredState) noexcept
{
    m_ResourceStateManager.TransitionSubresources(pView->m_pResource, pView->m_subresources, desiredState);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::TransitionResourceForBindings(ViewBase* pView) noexcept
{
    m_ResourceStateManager.TransitionSubresourcesForBindings(pView->m_pResource, pView->m_subresources);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::TransitionResourceForBindings(Resource* pResource) noexcept
{
    m_ResourceStateManager.TransitionResourceForBindings(pResource);
}

//----------------------------------------------------------------------------------------------------------------------------------
HRESULT TRANSLATION_API ImmediateContext::ResolveSharedResource(Resource* pResource)
{
    m_ResourceStateManager.TransitionResource(pResource, D3D12_RESOURCE_STATE_COMMON, COMMAND_LIST_TYPE::GRAPHICS, SubresourceTransitionFlags::StateMatchExact | SubresourceTransitionFlags::ForceExclusiveState | SubresourceTransitionFlags::NotUsedInCommandListIfNoStateChange);
    m_ResourceStateManager.ApplyAllResourceTransitions();

    // All work referencing this resource needs to be submitted, not necessarily completed.
    // Even reads / shared access needs to be flushed, because afterwards, another process can write to this resource.
    auto& CurrentState = pResource->m_Identity->m_currentState;
    for (UINT i = 0; i < (CurrentState.AreAllSubresourcesSame() ? 1u : pResource->NumSubresources()); ++i)
    {
        auto& ExclusiveState = CurrentState.GetExclusiveSubresourceState(i);
        assert(ExclusiveState.IsMostRecentlyExclusiveState && ExclusiveState.CommandListType == COMMAND_LIST_TYPE::GRAPHICS);
        if (ExclusiveState.FenceValue == GetCommandListID(ExclusiveState.CommandListType))
        {
            try {
                GetCommandListManager(COMMAND_LIST_TYPE::GRAPHICS)->PrepForCommandQueueSync(); // throws
            }
            catch (_com_error& e)
            {
                return e.Error();
            }
            catch (std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }
            break;
        }
    }
    return S_OK;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::GetSharedGDIHandle(_In_ Resource *pResource, _Out_ HANDLE *pHandle)
{
    assert(pResource->Parent()->IsGDIStyleHandleShared());
    ThrowFailure(m_pCompatDevice->ReflectSharedProperties(pResource->GetUnderlyingResource(), D3D12_REFLECT_SHARED_PROPERTY_NON_NT_SHARED_HANDLE, pHandle, sizeof(*pHandle)));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::CreateSharedNTHandle(_In_ Resource *pResource, _Out_ HANDLE *pHandle, _In_opt_ SECURITY_ATTRIBUTES *pSA)
{
    assert(pResource->Parent()->IsNTHandleShared()); // Note: Not validated by this layer, but only called when this is true.

    ThrowFailure(m_pDevice12->CreateSharedHandle(pResource->GetUnderlyingResource(), pSA, GENERIC_ALL, nullptr, pHandle));
}

//----------------------------------------------------------------------------------------------------------------------------------
bool RetiredObject::DeferredWaitsSatisfied(const std::vector<DeferredWait>& deferredWaits)
{
    for (auto& deferredWait : deferredWaits)
    {
        if (deferredWait.value > deferredWait.fence->GetCompletedValue())
        {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool RetiredObject::ReadyToDestroy(ImmediateContext* pContext, bool completionRequired, UINT64 lastCommandListID, COMMAND_LIST_TYPE commandListType, const std::vector<DeferredWait>& deferredWaits)
{
    bool readyToDestroy = true;

    if (completionRequired)
    {
        readyToDestroy = lastCommandListID <= pContext->GetCompletedFenceValue(commandListType);
    }
    else
    {
        readyToDestroy = lastCommandListID < pContext->GetCommandListID(commandListType);
    }

    if (readyToDestroy)
    {
        readyToDestroy = DeferredWaitsSatisfied(deferredWaits);
    }

    return readyToDestroy;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool RetiredObject::ReadyToDestroy(ImmediateContext* pContext, bool completionRequired, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], const std::vector<DeferredWait>& deferredWaits)
{
    bool readyToDestroy = true;

    if (completionRequired)
    {
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID  &&  readyToDestroy; i++)
        {
            if (lastCommandListIDs[i] != 0)
            {
                readyToDestroy = lastCommandListIDs[i] <= pContext->GetCompletedFenceValue((COMMAND_LIST_TYPE)i);
            }
        }
    }
    else
    {
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID  &&  readyToDestroy; i++)
        {
            if (lastCommandListIDs[i] != 0)
            {
                readyToDestroy = lastCommandListIDs[i] < pContext->GetCommandListID((COMMAND_LIST_TYPE)i);
            }
        }
    }

    if (readyToDestroy)
    {
        readyToDestroy = DeferredWaitsSatisfied(deferredWaits);
    }

    return readyToDestroy;
}

//----------------------------------------------------------------------------------------------------------------------------------
PipelineState* ImmediateContext::GetPipelineState()
{
    return m_CurrentState.m_pPSO;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetPipelineState(PipelineState* pPipeline)
{
    if (!m_CurrentState.m_pPSO || !pPipeline ||
         m_CurrentState.m_pPSO->GetRootSignature() != pPipeline->GetRootSignature())
    {
        m_DirtyStates |= e_GraphicsRootSignatureDirty | e_ComputeRootSignatureDirty;
    }

    m_CurrentState.m_pPSO = pPipeline;
    m_DirtyStates |= e_PipelineStateDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearInputBindings(Resource* pResource)
{
    if (pResource)
    {
        pResource->ClearInputBindings();
    }
}
//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::ClearOutputBindings(Resource* pResource)
{
    if (pResource)
    {
        pResource->ClearOutputBindings();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ClearVBBinding(UINT slot)
{
    if (m_CurrentState.m_VBs.UpdateBinding(slot, nullptr, e_Graphics))
    {
        m_DirtyStates |= e_VertexBuffersDirty;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ClearRTVBinding(UINT slot)
{
    if (m_CurrentState.m_RTVs.UpdateBinding(slot, nullptr, e_Graphics))
    {
        m_DirtyStates |= e_RenderTargetsDirty;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ClearDSVBinding()
{
    if (m_CurrentState.m_DSVs.UpdateBinding(0, nullptr, e_Graphics))
    {
        m_DirtyStates |= e_RenderTargetsDirty;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
DXGI_FORMAT ImmediateContext::GetParentForFormat(DXGI_FORMAT format)
{
    return CD3D11FormatHelper::GetParentFormat(format);
};

//----------------------------------------------------------------------------------------------------------------------------------
HRESULT TRANSLATION_API ImmediateContext::GetDeviceState()
{
    return m_pDevice12->GetDeviceRemovedReason();
}

//----------------------------------------------------------------------------------------------------------------------------------
TRANSLATION_API void ImmediateContext::Signal(
    _In_ Fence* pFence,
    UINT64 Value
    )
{
    if (m_pSyncOnlyQueue)
    {
        Flush(D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK);
        for (UINT listTypeIndex = 0; listTypeIndex < static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID); ++listTypeIndex)
        {
            auto pManager = m_CommandLists[listTypeIndex].get();
            if (!pManager)
            {
                continue;
            }
            UINT64 LastSignaledValue = pManager->GetCommandListID() - 1;
            ThrowFailure(m_pSyncOnlyQueue->Wait(pManager->GetFence()->Get(), LastSignaledValue));
            pFence->UsedInCommandList(static_cast<COMMAND_LIST_TYPE>(listTypeIndex), LastSignaledValue);
        }
        ThrowFailure(m_pSyncOnlyQueue->Signal(pFence->Get(), Value));
    }
    else
    {
        Flush(D3D12TranslationLayer::COMMAND_LIST_TYPE_GRAPHICS_MASK);
        ThrowFailure(GetCommandQueue(COMMAND_LIST_TYPE::GRAPHICS)->Signal(pFence->Get(), Value));
        pFence->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS) - 1);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
TRANSLATION_API void ImmediateContext::Wait(
    std::shared_ptr<Fence> const& pFence,
    UINT64 Value
    )
{
    if (pFence->DeferredWaits())
    {
        m_ResourceStateManager.AddDeferredWait(pFence, Value);
        return;
    }

    Flush(D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK);
    for (UINT listTypeIndex = 0; listTypeIndex < static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID); ++listTypeIndex)
    {
        auto pQueue = GetCommandQueue(static_cast<COMMAND_LIST_TYPE>(listTypeIndex));
        if (pQueue)
        {
            ThrowFailure(pQueue->Wait(pFence->Get(), Value));
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
unique_comptr<ID3D12Resource> ImmediateContext::AllocateHeap(UINT64 HeapSize, UINT64 alignment, AllocatorHeapType heapType) noexcept(false)
{
    D3D12_HEAP_PROPERTIES Props = GetHeapProperties(GetD3D12HeapType(heapType));
    D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(HeapSize, D3D12_RESOURCE_FLAG_NONE, alignment);
    unique_comptr<ID3D12Resource> spResource;
    HRESULT hr = m_pDevice12->CreateCommittedResource(
        &Props,
        D3D12_HEAP_FLAG_NONE,
        &Desc,
        GetDefaultPoolState(heapType),
        nullptr,
        IID_PPV_ARGS(&spResource));
    ThrowFailure(hr); // throw( _com_error )

    // Cache the Map within the D3D12 resource.
    CD3DX12_RANGE NullRange(0, 0);
    void* pData = nullptr;
    ThrowFailure(spResource->Map(0, &NullRange, &pData));

    return std::move(spResource);
}

//----------------------------------------------------------------------------------------------------------------------------------
void ImmediateContext::ClearState()
{
    m_CurrentState.ClearState();

    m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    m_uNumScissors = 0;
    m_uNumViewports = 0;
    m_ScissorRectEnable = false;

    memset(m_BlendFactor, 0, sizeof(m_BlendFactor));
    memset(m_auVertexOffsets, 0, sizeof(m_auVertexOffsets));
    memset(m_auVertexStrides, 0, sizeof(m_auVertexStrides));

    m_DirtyStates |= e_DirtyOnFirstCommandList;
    m_StatesToReassert |= e_ReassertOnNewCommandList;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SetMarker([[maybe_unused]] const wchar_t* name)
{
#ifdef USE_PIX
    PIXSetMarker(GetGraphicsCommandList(), 0, L"D3D11 Marker: %s", name);
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::BeginEvent([[maybe_unused]] const wchar_t* name)
{
#ifdef USE_PIX
    PIXBeginEvent(GetGraphicsCommandList(), 0, L"D3D11 Event: %s", name);
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::EndEvent()
{
#ifdef USE_PIX
    PIXEndEvent(GetGraphicsCommandList());
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::SharingContractPresent(_In_ Resource* pResource)
{
    Flush(COMMAND_LIST_TYPE_GRAPHICS_MASK);

    auto pSharingContract = GetCommandListManager(COMMAND_LIST_TYPE::GRAPHICS)->GetSharingContract();
    if (pSharingContract)
    {
        ID3D12Resource* pUnderlying = pResource->GetUnderlyingResource();
        pSharingContract->Present(pUnderlying, 0, nullptr);
    }

    pResource->UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
    GetCommandListManager(COMMAND_LIST_TYPE::GRAPHICS)->SetNeedSubmitFence();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API ImmediateContext::Present(
    _In_reads_(uSrcSurfaces) PresentSurface const* pSrcSurfaces,
    UINT numSrcSurfaces,
    _In_opt_ Resource* pDest,
    UINT flipInterval,
    UINT vidPnSourceId,
    _In_ D3DKMT_PRESENT* pKMTPresent,
    bool bDoNotSequence,
    std::function<HRESULT(PresentCBArgs&)> pfnPresentCb)
{
    if (bDoNotSequence)
    {
        // Blt with DoNotSequence is not supported in DX9/DX11. Supporting this would require extra tracking to
        // ensure defer deletion works correctly
        if (pDest)
        {
            ThrowFailure(E_INVALIDARG);
        }
    }
    else
    {
        if (!pKMTPresent->Flags.RedirectedFlip)
        {
            m_MaxFrameLatencyHelper.WaitForMaximumFrameLatency();
        }

        PresentSurface PresentOverride;
        if (pDest)
        {
            assert(numSrcSurfaces == 1);
            Resource* pSource = pSrcSurfaces->m_pResource;
            if (pSource->AppDesc()->Samples() > 1)
            {
                Resource* pTemp = m_BltResolveManager.GetBltResolveTempForWindow(pKMTPresent->hWindow, *pSource);
                ResourceResolveSubresource(pTemp, 0, pSource, pSrcSurfaces->m_subresource, pSource->AppDesc()->Format());
                PresentOverride.m_pResource = pTemp;
                PresentOverride.m_subresource = 0;
                numSrcSurfaces = 1;
                pSrcSurfaces = &PresentOverride;
            }
        }

        for (UINT i = 0; i < numSrcSurfaces; i++)
        {
            const UINT appSubresource = pSrcSurfaces[i].m_subresource;
            Resource* pResource = pSrcSurfaces[i].m_pResource;

            for (UINT iPlane = 0; iPlane < pResource->AppDesc()->NonOpaquePlaneCount(); ++iPlane)
            {
                UINT subresourceIndex = ConvertSubresourceIndexAddPlane(appSubresource, pResource->AppDesc()->SubresourcesPerPlane(), iPlane);

                GetResourceStateManager().TransitionSubresource(pResource,
                    subresourceIndex,
                    D3D12_RESOURCE_STATE_PRESENT,
                    COMMAND_LIST_TYPE::GRAPHICS,
                    SubresourceTransitionFlags::StateMatchExact);
            }
        }

        if (pDest)
        {
            GetResourceStateManager().TransitionResource(pDest, D3D12_RESOURCE_STATE_COPY_DEST);
        }
        
        GetResourceStateManager().ApplyAllResourceTransitions();

        PresentCBArgs presentArgs = {};
        presentArgs.pGraphicsCommandQueue = GetCommandQueue(COMMAND_LIST_TYPE::GRAPHICS);
        presentArgs.pGraphicsCommandList = GetCommandList(COMMAND_LIST_TYPE::GRAPHICS);
        presentArgs.pSrcSurfaces = pSrcSurfaces;
        presentArgs.numSrcSurfaces = numSrcSurfaces;
        presentArgs.pDest = pDest;
        presentArgs.flipInterval = flipInterval;
        presentArgs.vidPnSourceId = vidPnSourceId;
        presentArgs.pKMTPresent = pKMTPresent;

        ThrowFailure(pfnPresentCb(presentArgs));

        GetCommandListManager(COMMAND_LIST_TYPE::GRAPHICS)->PrepForCommandQueueSync(); // throws
    }
}

HRESULT TRANSLATION_API ImmediateContext::CloseAndSubmitGraphicsCommandListForPresent(
    BOOL commandsAdded,
    _In_reads_(numSrcSurfaces) const PresentSurface* pSrcSurfaces,
    UINT numSrcSurfaces,
    _In_opt_ Resource* pDest,
    _In_ D3DKMT_PRESENT* pKMTPresent)
{
    const auto commandListType = COMMAND_LIST_TYPE::GRAPHICS;
    if (commandsAdded)
    {
        AdditionalCommandsAdded(commandListType);
    }
    UINT commandListMask = D3D12TranslationLayer::COMMAND_LIST_TYPE_GRAPHICS_MASK;
    if (!Flush(commandListMask))
    {
        CloseCommandList(commandListMask);
        ResetCommandList(commandListMask);
    }

    auto pSharingContract = GetCommandListManager(commandListType)->GetSharingContract();
    if (pSharingContract)
    {
        for (UINT i = 0; i < numSrcSurfaces; ++i)
        {
            pSharingContract->Present(pSrcSurfaces[i].m_pResource->GetUnderlyingResource(), pSrcSurfaces[i].m_subresource, pKMTPresent->hWindow);
        }
    }

    // These must be marked after the flush so that they are defer deleted
    // Don't mark these for residency management as these aren't part of the next command list
    UINT64 CommandListID = GetCommandListID(commandListType);
    if (pDest)
    {
        pDest->UsedInCommandList(commandListType, CommandListID);
    }
    for (UINT i = 0; i < numSrcSurfaces; i++)
    {
        D3D12TranslationLayer::Resource* pResource = pSrcSurfaces[i].m_pResource;
        pResource->UsedInCommandList(commandListType, CommandListID);
    }
    if (!pKMTPresent->Flags.RedirectedFlip)
    {
        m_MaxFrameLatencyHelper.RecordPresentFenceValue(CommandListID);
    }

    return S_OK;
}

//----------------------------------------------------------------------------------------------------------------------------------
ImmediateContext::BltResolveManager::BltResolveManager(D3D12TranslationLayer::ImmediateContext& ImmCtx)
    : m_ImmCtx(ImmCtx)
{
}

//----------------------------------------------------------------------------------------------------------------------------------
Resource* ImmediateContext::BltResolveManager::GetBltResolveTempForWindow(HWND hwnd, Resource& presentingResource)
{
    auto& spTemp = m_Temps[hwnd];
    auto pResourceDesc = presentingResource.Parent();
    if (spTemp)
    {
        if (spTemp->AppDesc()->Format() != pResourceDesc->m_appDesc.Format() ||
            spTemp->AppDesc()->Width() != pResourceDesc->m_appDesc.Width() ||
            spTemp->AppDesc()->Height() != pResourceDesc->m_appDesc.Height())
        {
            spTemp.reset();
        }
    }
    if (!spTemp)
    {
        auto Desc = *pResourceDesc;
        Desc.m_appDesc.m_Samples = 1;
        Desc.m_appDesc.m_Quality = 0;
        Desc.m_desc12.SampleDesc.Count = 1;
        Desc.m_desc12.SampleDesc.Quality = 0;

        spTemp = Resource::CreateResource(&m_ImmCtx, Desc, ResourceAllocationContext::ImmediateContextThreadLongLived);
    }
    return spTemp.get();
}

}
