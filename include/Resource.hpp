// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{

    // The same as D3D11
    enum RESOURCE_USAGE
    {
        RESOURCE_USAGE_DEFAULT = 0,
        RESOURCE_USAGE_IMMUTABLE = 1,
        RESOURCE_USAGE_DYNAMIC = 2,
        RESOURCE_USAGE_STAGING = 3,
    };

    enum RESOURCE_CPU_ACCESS
    {
        RESOURCE_CPU_ACCESS_NONE = 0x00000L,
        RESOURCE_CPU_ACCESS_WRITE = 0x10000L,
        RESOURCE_CPU_ACCESS_READ = 0x20000L,
    };
    DEFINE_ENUM_FLAG_OPERATORS( RESOURCE_CPU_ACCESS );

    enum RESOURCE_BIND_FLAGS
    {
        RESOURCE_BIND_NONE = 0x0L,
        RESOURCE_BIND_VERTEX_BUFFER = 0x1L,
        RESOURCE_BIND_INDEX_BUFFER = 0x2L,
        RESOURCE_BIND_CONSTANT_BUFFER = 0x4L,
        RESOURCE_BIND_SHADER_RESOURCE = 0x8L,
        RESOURCE_BIND_STREAM_OUTPUT = 0x10L,
        RESOURCE_BIND_RENDER_TARGET = 0x20L,
        RESOURCE_BIND_DEPTH_STENCIL = 0x40L,
        RESOURCE_BIND_UNORDERED_ACCESS = 0x80L,
        RESOURCE_BIND_GPU_INPUT = 0x20fL,
        RESOURCE_BIND_GPU_OUTPUT = 0xf0L,
        RESOURCE_BIND_CAPTURE = 0x800L,
        RESOURCE_BIND_DECODER = 0x200L,
        RESOURCE_BIND_VIDEO_ENCODER = 0x400L,
    };
    DEFINE_ENUM_FLAG_OPERATORS( RESOURCE_BIND_FLAGS );

    enum MAP_TYPE
    {
        MAP_TYPE_READ = 1,
        MAP_TYPE_WRITE = 2,
        MAP_TYPE_READWRITE = 3,
        MAP_TYPE_WRITE_DISCARD = 4,
        MAP_TYPE_WRITE_NOOVERWRITE = 5,
    };

    enum class DeferredDestructionType
    {
        Submission,
        Completion
    };

    struct MappedSubresource
    {
        void * pData;
        UINT RowPitch;
        UINT DepthPitch;
    };

    // The parameters of the resource as the app sees it, the translation layer
    // can alter the values under the covers hence the distinction.
    struct AppResourceDesc
    {
        AppResourceDesc() { memset(this, 0, sizeof(*this)); }

        AppResourceDesc(UINT SubresourcesPerPlane,
            UINT8   NonOpaquePlaneCount,
            UINT    Subresources,
            UINT8   MipLevels,
            UINT16  ArraySize,
            UINT    Depth,
            UINT    Width,
            UINT    Height,
            DXGI_FORMAT Format,
            UINT    Samples,
            UINT    Quality,
            RESOURCE_USAGE usage,
            RESOURCE_CPU_ACCESS cpuAcess,
            RESOURCE_BIND_FLAGS bindFlags,
            D3D12_RESOURCE_DIMENSION dimension) :
                    m_SubresourcesPerPlane(SubresourcesPerPlane)
                    ,m_NonOpaquePlaneCount(NonOpaquePlaneCount)
                    ,m_Subresources(Subresources)
                    ,m_MipLevels(MipLevels)
                    ,m_ArraySize(ArraySize)
                    ,m_Depth(Depth)
                    ,m_Width(Width)
                    ,m_Height(Height)
                    ,m_Format(Format)
                    ,m_Samples(Samples)
                    ,m_Quality(Quality)
                    ,m_usage(usage)
                    ,m_cpuAcess(cpuAcess)
                    ,m_bindFlags(bindFlags)
                    ,m_resourceDimension(dimension) {}

        AppResourceDesc(const D3D12_RESOURCE_DESC &desc12, 
            D3D12TranslationLayer::RESOURCE_USAGE Usage, 
            DWORD Access, 
            DWORD BindFlags);

        UINT SubresourcesPerPlane() const { return m_SubresourcesPerPlane; }
        UINT8 NonOpaquePlaneCount() const { return m_NonOpaquePlaneCount; }
        UINT Subresources() const { return m_Subresources; }
        UINT8 MipLevels() const { return m_MipLevels; }
        UINT16 ArraySize() const { return m_ArraySize; }
        UINT Depth() const { return m_Depth; }
        UINT Width() const { return m_Width; }
        UINT Height() const { return m_Height; }
        DXGI_FORMAT Format() const { return m_Format; }
        UINT Samples() const { return m_Samples; }
        UINT Quality() const { return m_Quality; }
        RESOURCE_CPU_ACCESS CPUAccessFlags() const { return m_cpuAcess; }
        RESOURCE_USAGE Usage() const { return m_usage; }
        RESOURCE_BIND_FLAGS BindFlags() const { return m_bindFlags; }
        D3D12_RESOURCE_DIMENSION ResourceDimension() const { return m_resourceDimension; }

        UINT    m_SubresourcesPerPlane;
        UINT8   m_NonOpaquePlaneCount;
        UINT    m_Subresources;
        UINT8   m_MipLevels;
        UINT16  m_ArraySize;
        UINT    m_Depth;
        UINT    m_Width;
        UINT    m_Height;
        DXGI_FORMAT m_Format;
        UINT    m_Samples;
        UINT    m_Quality;
        RESOURCE_USAGE m_usage;
        RESOURCE_CPU_ACCESS m_cpuAcess;
        RESOURCE_BIND_FLAGS m_bindFlags;
        D3D12_RESOURCE_DIMENSION m_resourceDimension;
    };

    enum class FormatEmulation
    {
        None = 0,
        YV12
    };

    //TODO: remove this once runtime dependecy has been removed
    struct ResourceCreationArgs
    {
        D3D12_RESOURCE_DIMENSION ResourceDimension12() const { return m_desc12.Dimension; }
        D3D12_TEXTURE_LAYOUT ApiTextureLayout12() const { return m_desc12.Layout; }

        D3D11_RESOURCE_DIMENSION ResourceDimension11() const
        {
            return static_cast<D3D11_RESOURCE_DIMENSION>(ResourceDimension12());
        }

        UINT ArraySize() const
        {
            return (ResourceDimension12() == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : m_desc12.DepthOrArraySize;
        }

        bool IsShared() const
        {
            return (m_flags11.MiscFlags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)) != 0;
        }
        bool IsNTHandleShared() const
        {
            return (m_flags11.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) != 0;
        }
        bool IsGDIStyleHandleShared() const
        {
            assert(!(IsNTHandleShared() && !IsShared())); // Can't be NT handle shared and not regular shared
            return IsShared() && !IsNTHandleShared();
        }

        D3D12_RESOURCE_DESC m_desc12;

        AppResourceDesc m_appDesc;

        D3D12_HEAP_DESC m_heapDesc;
        D3D11_RESOURCE_FLAGS m_flags11;

        bool m_isPlacedTexture;

        bool m_bBoundForStreamOut;
        bool m_bManageResidency;
        bool m_bTriggerDeferredWaits;
        bool m_bIsD3D9on12Resource;

        FormatEmulation m_FormatEmulation = FormatEmulation::None;

        // Setting this function overrides the normal creation method used by the translation layer.
        // It can be used for smuggling a resource through the create path or using alternate creation APIs.
        std::function<void(ResourceCreationArgs const&, ID3D12SwapChainAssistant*, ID3D12Resource**)> m_PrivateCreateFn;

        //11on12 Only
        UINT m_OffsetToStreamOutputSuffix;

        AllocatorHeapType m_heapType = AllocatorHeapType::None;
    };

    // Handles when to start and end tracking for D3DX12ResidencyManager::ManagedObject
    struct ResidencyManagedObjectWrapper
    {
        ResidencyManagedObjectWrapper(ResidencyManager &residencyManager) : m_residencyManager(residencyManager) {}

        // ManagedObject uses a type of linked lists that breaks when copying it around, so disable the copy operator
        ResidencyManagedObjectWrapper(const ResidencyManagedObjectWrapper&) = delete;

        void Initialize(ID3D12Pageable *pResource, UINT64 resourceSize, bool isResident = true)
        {
            m_residencyHandle.Initialize(pResource, resourceSize);
            if (!isResident)
            {
                m_residencyHandle.ResidencyStatus = ManagedObject::RESIDENCY_STATUS::EVICTED;
            }
            m_residencyManager.BeginTrackingObject(&m_residencyHandle);
        }

        ~ResidencyManagedObjectWrapper()
        {
            m_residencyManager.EndTrackingObject(&m_residencyHandle);
        }
        ManagedObject &GetManagedObject() { return m_residencyHandle; }
    private:
        ManagedObject m_residencyHandle;
        ResidencyManager &m_residencyManager;
    };

    // Wraps the BufferSuballocation with functions that help automatically account for the 
    // suballocated offset
    class D3D12ResourceSuballocation
    {
    public:
        D3D12ResourceSuballocation() { Reset(); }
        D3D12ResourceSuballocation(ID3D12Resource *pResource, const HeapSuballocationBlock &allocation) :
            m_pResource(pResource), m_bufferSubAllocation(allocation) {}

        bool IsInitialized() { return GetResource() != nullptr; }
        void Reset() { m_pResource = nullptr; }
        ID3D12Resource *GetResource() const { return m_pResource; }
        UINT64 GetOffset() const {

            UINT offset = 0;
            if (m_bufferSubAllocation.IsDirectAllocation())
            {

                assert(m_bufferSubAllocation.GetOffset() == 0);
            }
            else
            {
                // The disjoint buddy allocator works as if all the resources were 
                // one contiguous block of memory and the offsets reflect this.
                // Convert the offset to be local to the selected resource
                offset = m_bufferSubAllocation.GetOffset() % cBuddyAllocatorThreshold;
            }
            return offset;
        }

        HRESULT Map(
            UINT Subresource,
            _In_opt_  const D3D12_RANGE *pReadRange,
            _Outptr_opt_result_bytebuffer_(_Inexpressible_("Dependent on resource"))  void **ppData)
        {
            D3D12_RANGE *pOffsetReadRange = nullptr;
            D3D12_RANGE offsetReadRange;

            if (pReadRange)
            {
                offsetReadRange = OffsetRange(*pReadRange);
                pOffsetReadRange = &offsetReadRange;
            }
            HRESULT hr = GetResource()->Map(Subresource, pOffsetReadRange, ppData);

            if (*ppData)
            {
                *ppData = (void*)((BYTE *)(*ppData) + GetOffset());
            }

            return hr;
        }

        void Unmap(
            UINT Subresource,
            _In_opt_  const D3D12_RANGE *pWrittenRange)
        {
            D3D12_RANGE *pOffsetWrittenRange = nullptr;
            D3D12_RANGE offsetWrittenRange;

            if (pWrittenRange)
            {
                offsetWrittenRange = OffsetRange(*pWrittenRange);
                pOffsetWrittenRange = &offsetWrittenRange;
            }
            GetResource()->Unmap(Subresource, pOffsetWrittenRange);
        }

        D3D12_TEXTURE_COPY_LOCATION GetCopyLocation(const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint) const
        {
            CD3DX12_TEXTURE_COPY_LOCATION copyLocation(GetResource(), footprint);
            copyLocation.PlacedFootprint.Offset += GetOffset();
            return copyLocation;
        }

        HeapSuballocationBlock const& GetBufferSuballocation() const
        {
            return m_bufferSubAllocation;
        }
        HeapSuballocationBlock &GetBufferSuballocation()
        {
            return m_bufferSubAllocation;
        }

        inline D3D12_RANGE OffsetRange(const D3D12_RANGE &originalRange) const
        {
            // If the range is empty, just leave it as-is
            if (originalRange.Begin == 0 && originalRange.End == 0)
            {
                return originalRange;
            }

            D3D12_RANGE offsetRange;
            offsetRange.Begin = originalRange.Begin + (SIZE_T)GetOffset();
            offsetRange.End = originalRange.End + (SIZE_T)GetOffset();
            return offsetRange;
        }
    private:
        ID3D12Resource *m_pResource;
        HeapSuballocationBlock m_bufferSubAllocation;
    };

    struct EncodedResourceSuballocation
    {
    private:
        UINT64 Offset;
        UINT64 Size;
        SIZE_T Ptr;
        static constexpr UINT c_DirectAllocationMask = 1u;
        static UINT GetDirectAllocationMask(HeapSuballocationBlock const& block)
        {
            return block.IsDirectAllocation() ? c_DirectAllocationMask : 0u;
        }

    public:
        EncodedResourceSuballocation() = default;
        EncodedResourceSuballocation(HeapSuballocationBlock const& block, ID3D12Resource* pPtr)
            : Offset(block.GetOffset())
            , Size(block.GetSize())
            , Ptr(reinterpret_cast<SIZE_T>(pPtr) | GetDirectAllocationMask(block))
        {
        }
        EncodedResourceSuballocation(D3D12ResourceSuballocation const& suballoc)
            : EncodedResourceSuballocation(suballoc.GetBufferSuballocation(), suballoc.GetResource())
        {
        }
        bool IsDirectAllocation() const { return (Ptr & c_DirectAllocationMask) != 0; }
        ID3D12Resource* GetResource() const { return reinterpret_cast<ID3D12Resource*>(Ptr & ~SIZE_T(1)); }
        ID3D12Resource* GetDirectAllocation() const { return IsDirectAllocation() ? GetResource() : nullptr; }
        HeapSuballocationBlock DecodeSuballocation() const { return HeapSuballocationBlock(Offset, Size, GetDirectAllocation()); }
        D3D12ResourceSuballocation Decode() const { return D3D12ResourceSuballocation(GetResource(), DecodeSuballocation()); }
    };

    struct OutstandingResourceUse
    {
        OutstandingResourceUse(COMMAND_LIST_TYPE type, UINT64 value)
        {
            commandListType = type;
            fenceValue = value;
        }
        COMMAND_LIST_TYPE commandListType;
        UINT64 fenceValue;

        bool operator==(OutstandingResourceUse const& rhs) const
        {
            return commandListType == rhs.commandListType &&
                   fenceValue == rhs.fenceValue;
        }
    };

    //==================================================================================================================================
    // Resource
    // Stores data responsible for remapping D3D11 resources to underlying D3D12 resources and heaps
    //==================================================================================================================================
    class Resource : public DeviceChild,
        public TransitionableResourceBase
    {
    public: // Methods
        friend class ImmediateContext;
        friend class BatchedResource;

    private:
        Resource(ImmediateContext* pDevice, ResourceCreationArgs& createArgs, void*& pPreallocatedMemory) noexcept(false);
        ~Resource() noexcept;

        void Create(ResourceAllocationContext threadingContext) noexcept(false);

        static size_t CalcPreallocationSize(ResourceCreationArgs const& createArgs);
        unique_comptr<Resource> static AllocateResource(ImmediateContext* pDevice, ResourceCreationArgs& createArgs) noexcept(false);

        volatile UINT m_RefCount = 0;

    public:
        static TRANSLATION_API unique_comptr<Resource> CreateResource(ImmediateContext* pDevice, ResourceCreationArgs& createArgs, ResourceAllocationContext threadingContext) noexcept(false);
        static TRANSLATION_API unique_comptr<Resource> OpenResource(ImmediateContext* pDevice,
            ResourceCreationArgs& createArgs,
            _In_ IUnknown *pResource,
            DeferredDestructionType deferredDestructionType,
            _In_ D3D12_RESOURCE_STATES currentState) noexcept(false);


        inline void AddRef() { InterlockedIncrement(&m_RefCount); }
        inline void Release() { if (InterlockedDecrement(&m_RefCount) == 0) { delete this; } }

        void UsedInCommandList(COMMAND_LIST_TYPE commandListType, UINT64 id);

        ResourceCreationArgs* Parent() { return &m_creationArgs; }
        AppResourceDesc* AppDesc() { return &m_creationArgs.m_appDesc; }

        ID3D12Resource* GetUnderlyingResource() noexcept { return m_Identity->GetResource(); }
        void UnderlyingResourceChanged() noexcept(false);
        void ZeroConstantBufferPadding() noexcept;

        UINT NumSubresources() noexcept { return AppDesc()->Subresources() * m_SubresourceMultiplier; }
        UINT8 SubresourceMultiplier() noexcept { return m_SubresourceMultiplier; }
        UINT GetExtendedSubresourceIndex(UINT Index, UINT Plane) noexcept
        {
            assert(AppDesc()->NonOpaquePlaneCount() == 1 || Plane == 0);
            return ConvertSubresourceIndexAddPlane(Index, AppDesc()->SubresourcesPerPlane(), Plane);
        }
        CSubresourceSubset GetFullSubresourceSubset() { return CSubresourceSubset(AppDesc()->MipLevels(), AppDesc()->ArraySize(), AppDesc()->NonOpaquePlaneCount() * m_SubresourceMultiplier); }

        void DecomposeSubresource(_In_ UINT Subresource, _Out_ UINT &mipSlice, _Out_ UINT &arraySlice, _Out_ UINT &planeSlice) 
        { 
            D3D12DecomposeSubresource(Subresource, Parent()->m_desc12.MipLevels, Parent()->ArraySize(), mipSlice, arraySlice, planeSlice);
        }

        UINT GetSubresourceIndex(UINT PlaneIndex, UINT MipLevel, UINT ArraySlice) { 
            return D3D12CalcSubresource(MipLevel, ArraySlice, PlaneIndex, Parent()->m_desc12.MipLevels, Parent()->ArraySize());
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT& GetSubresourcePlacement(UINT subresource) noexcept;
        D3D12_RANGE GetSubresourceRange(UINT subresource, _In_opt_ const D3D12_BOX *pSelectedBox = nullptr) noexcept;
        UINT64 GetResourceSize() noexcept;
        static D3D12_HEAP_TYPE GetD3D12HeapType(RESOURCE_USAGE usage, UINT cpuAccessFlags) noexcept;

        static void FillSubresourceDesc(ID3D12Device* pDevice, DXGI_FORMAT, UINT Width, UINT Height, UINT Depth, _Out_ D3D12_PLACED_SUBRESOURCE_FOOTPRINT& Placement) noexcept;
        UINT DepthPitch(UINT Subresource) noexcept;

        template<typename TViewIface> UINT GetUniqueness() const noexcept { return m_AllUniqueness; }
        template<> UINT GetUniqueness<ShaderResourceViewType>() const noexcept { return m_SRVUniqueness; }

        template<typename TViewIface> void ViewBound(View<TViewIface>* pView, EShaderStage stage, UINT slot) { m_currentBindings.ViewBound(pView, stage, slot); }
        template<typename TViewIface> void ViewUnbound(View<TViewIface>* pView, EShaderStage stage, UINT slot) { m_currentBindings.ViewUnbound(pView, stage, slot); }

        inline UINT GetOffsetToStreamOutputSuffix() { return m_OffsetToStreamOutputSuffix; }
        RESOURCE_USAGE GetEffectiveUsage() const { return m_effectiveUsage; }
        inline bool IsBloatedConstantBuffer() { return (AppDesc()->BindFlags() & RESOURCE_BIND_FLAGS::RESOURCE_BIND_CONSTANT_BUFFER) && AppDesc()->Width() % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0; }
        inline bool IsDefaultResourceBloated() { return m_OffsetToStreamOutputSuffix != 0 || IsBloatedConstantBuffer(); }
        inline bool TriggersDeferredWaits() const { return m_creationArgs.m_bTriggerDeferredWaits; }
        inline FormatEmulation GetFormatEmulation() const { return m_creationArgs.m_FormatEmulation; }
        inline bool IsInplaceFormatEmulation() const { return GetFormatEmulation() == FormatEmulation::YV12; }  // Format emulation modifies the resource in place for map/unmap

        bool WaitForOutstandingResourcesIfNeeded(bool DoNotWait);

        void AddHeapToTilePool(unique_comptr<ID3D12Heap> spHeap)
        {
            auto HeapDesc = spHeap->GetDesc();
            m_TilePool.m_Allocations.emplace_back(static_cast<UINT>(HeapDesc.SizeInBytes), 0); // throw( bad_alloc )
            auto& Allocation = m_TilePool.m_Allocations.front();
            Allocation.m_spUnderlyingBufferHeap = std::move(spHeap);
        }

        inline void SetMinLOD(float MinLOD) { m_MinLOD = MinLOD; }
        inline float GetMinLOD() { return m_MinLOD; }

        inline void SetWaitForCompletionRequired(bool value) { m_bWaitForCompletionRequired = value; }
        void ClearInputBindings();
        void ClearOutputBindings();

        UINT GetCommandListTypeMaskFromUsed()
        {
            UINT typeMask = 0;
            for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
            {
                if (m_LastUsedCommandListID[i] != 0)
                {
                    typeMask |= (1 << i);
                }
            }
            return typeMask;
        }

        UINT GetCommandListTypeMask()
        {
            UINT typeMask = m_Identity->m_currentState.GetCommandListTypeMask();
            if (typeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
            {
                typeMask = GetCommandListTypeMaskFromUsed();
            }
            return typeMask;
        }

        UINT GetCommandListTypeMask(const CViewSubresourceSubset &viewSubresources)
        {
            UINT typeMask = m_Identity->m_currentState.GetCommandListTypeMask(viewSubresources);
            if (typeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
            {
                typeMask = GetCommandListTypeMaskFromUsed();
            }
            return typeMask;
        }

        UINT GetCommandListTypeMask(UINT Subresource)
        {
            UINT typeMask = m_Identity->m_currentState.GetCommandListTypeMask(Subresource);
            if (typeMask == COMMAND_LIST_TYPE_UNKNOWN_MASK)
            {
                typeMask = GetCommandListTypeMaskFromUsed();
            }
            return typeMask;
        }


        // Used for when we are reusing a generic buffer that's used as an intermediate copy resource. Because
        // we're constantly copying to/from different resources with different footprints, we need to make sure we 
        // update the app desc so that copies will use the right footprint
        void UpdateAppDesc(const AppResourceDesc &AppDesc);

        void SwapIdentities(Resource& Other)
        {
            std::swap(m_Identity, Other.m_Identity);
            std::swap(m_SubresourcePlacement[0].Offset, Other.m_SubresourcePlacement[0].Offset);
            DeviceChild::SwapIdentities(Other);
        }

        void AddToResidencyManager(bool bIsResident);

        bool IsResident()
        {
            return !GetIdentity()->m_pResidencyHandle ||
                GetIdentity()->m_pResidencyHandle->GetManagedObject().ResidencyStatus == ManagedObject::RESIDENCY_STATUS::RESIDENT;
        }

        static bool IsSuballocatedFromSameHeap(Resource *pResourceA, Resource *pResourceB)
        {
            return pResourceA && pResourceB && pResourceA->GetIdentity()->m_bOwnsUnderlyingResource == false && pResourceB->GetIdentity()->m_bOwnsUnderlyingResource == false &&
                pResourceA->GetIdentity()->GetSuballocatedResource() == pResourceB->GetIdentity()->GetSuballocatedResource();
        }

        static bool IsSameUnderlyingSubresource(Resource *pResourceA, UINT subresourceA, Resource *pResourceB, UINT subresourceB)
        {
            return pResourceA && pResourceB && 
                ((pResourceA == pResourceB && subresourceA == subresourceB) || IsSuballocatedFromSameHeap(pResourceA, pResourceB));
        }

        AllocatorHeapType GetAllocatorHeapType()
        {
            assert(AppDesc()->CPUAccessFlags() != 0);

            if (m_creationArgs.m_heapType == AllocatorHeapType::None)
            {
                if (IsDecoderCompressedBuffer())
                {
                    return AllocatorHeapType::Decoder;
                }
                else if (AppDesc()->CPUAccessFlags() & RESOURCE_CPU_ACCESS_READ)
                {
                    return AllocatorHeapType::Readback;
                }

                return AllocatorHeapType::Upload;
            }

            return m_creationArgs.m_heapType;
        }

        bool UnderlyingResourceIsSuballocated()
        {
            return !m_Identity->m_bOwnsUnderlyingResource && m_spCurrentCpuHeaps.size() == 0;
        }

        bool IsLockableSharedBuffer()
        {
            return m_creationArgs.m_desc12.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && m_creationArgs.IsShared() && AppDesc()->CPUAccessFlags();
        }

        bool IsDecoderCompressedBuffer()
        {
            return Parent()->ResourceDimension12() == D3D12_RESOURCE_DIMENSION_BUFFER && (AppDesc()->BindFlags() &  RESOURCE_BIND_DECODER);
        }

        bool OwnsReadbackHeap()
        {
            // These are cases where we can't suballocate out of larger heaps because resource transitions can only be done on heap granularity 
            // and these resources can be transitioned out of the default heap state (COPY_DEST).
            // 
            // Note: We don't need to do this for dynamic write-only buffers because those buffers always stay in GENRIC_READ and only transition 
            // at copies (and transition back to GENERIC read directly afterwards) 
            if (IsDecoderCompressedBuffer())
            {
                return false;
            }
            else
            {
                return AppDesc()->BindFlags() &  RESOURCE_BIND_DECODER ||
                       m_creationArgs.m_heapDesc.Properties.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE ||
                       (AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC && Parent()->ResourceDimension12() == D3D12_RESOURCE_DIMENSION_BUFFER && (AppDesc()->CPUAccessFlags() & RESOURCE_CPU_ACCESS_READ) != 0);
            }
        }

    private:
        void InitializeSubresourceDescs() noexcept(false);
        void InitializeTilingData() noexcept;

        void CreateUnderlying(ResourceAllocationContext threadingContext) noexcept(false);

    public:
        // Contains all information that can be rotatable for back buffers
        struct SResourceIdentity
        {
            SResourceIdentity(UINT NumSubresources, bool bSimultaneousAccess, void*& pPreallocatedMemory) noexcept(false)
                : m_currentState(NumSubresources, bSimultaneousAccess, pPreallocatedMemory)
            {
            }

            unique_comptr<ID3D12Resource> m_spUnderlyingResource;
            D3D12ResourceSuballocation m_suballocation;

            ID3D12Resource *GetOwnedResource()
            {
                assert(m_bOwnsUnderlyingResource);
                return m_spUnderlyingResource.get();
            }

            ID3D12Resource *GetSuballocatedResource()
            {
                assert(!m_bOwnsUnderlyingResource);
                return m_suballocation.GetResource();
            }

            UINT64 GetSuballocatedOffset()
            {
                assert(!m_bOwnsUnderlyingResource);
                return m_suballocation.GetOffset();
            }

            ID3D12Resource *GetResource()
            {
                if (m_bOwnsUnderlyingResource)
                {
                    return GetOwnedResource();
                }
                else
                {
                    return GetSuballocatedResource();
                }
            }

            bool m_bOwnsUnderlyingResource = true;
            bool m_bSharedResource = false;
            bool m_bPlacedTexture = false;

            CCurrentResourceState m_currentState;
            std::unique_ptr<ResidencyManagedObjectWrapper> m_pResidencyHandle;

            UINT64 m_LastUAVAccess = 0;

            bool HasRestrictedOutstandingResources()
            {
                return m_MaxOutstandingResources != 0xffffffff;
            }

            std::vector<OutstandingResourceUse> m_OutstandingResources;
            UINT m_MaxOutstandingResources = 0xffffffff;
        };

        std::unique_ptr<SResourceIdentity> AllocateResourceIdentity(UINT NumSubresources, bool bSimultaneousAccess)
        {
            struct VoidDeleter { void operator()(void* p) { operator delete(p); } };

            size_t ObjectSize = sizeof(SResourceIdentity) + CCurrentResourceState::CalcPreallocationSize(NumSubresources, bSimultaneousAccess);
            std::unique_ptr<void, VoidDeleter> spMemory(operator new(ObjectSize));

            void* pPreallocatedMemory = reinterpret_cast<SResourceIdentity*>(spMemory.get()) + 1;
            std::unique_ptr<SResourceIdentity> spIdentity(
                new (spMemory.get()) SResourceIdentity(NumSubresources, bSimultaneousAccess, pPreallocatedMemory));
            spMemory.release();

            // This can fire if CalcPreallocationSize doesn't account for all preallocated arrays, or if the void*& decays to void*
            assert(reinterpret_cast<size_t>(pPreallocatedMemory) == reinterpret_cast<size_t>(spIdentity.get()) + ObjectSize);

            return std::move(spIdentity);
        }

        SResourceIdentity* GetIdentity() { return m_Identity.get(); }
        CResourceBindings& GetBindingState() { return m_currentBindings; }

        ManagedObject *GetResidencyHandle();
    private:
        template <typename ViewType, typename UnbindFunction>
        void UnbindList(LIST_ENTRY list, UnbindFunction& unbindFunction)
        {
            for (LIST_ENTRY *pListEntry = list.Flink; pListEntry != &list;)
            {
                auto pViewBindings = CONTAINING_RECORD(pListEntry, CViewBindings<ViewType>, m_ViewBindingList);

                for (UINT stage = 0; stage < _countof(pViewBindings->m_BindPoints); ++stage)
                {
                    auto& bindings = pViewBindings->m_BindPoints[stage];
                    for (UINT slot = 0; bindings.any(); ++slot)
                    {
                        if (bindings.test(slot))
                        {
                            unbindFunction(stage, slot);
                        }
                    }
                }

                D3D12TranslationLayer::RemoveEntryList(&pViewBindings->m_ViewBindingList);
                D3D12TranslationLayer::InitializeListHead(&pViewBindings->m_ViewBindingList);

                if (D3D12TranslationLayer::IsListEmpty(pListEntry))
                {
                    break;
                }
            }
        }

        void UnBindAsRTV();
        void UnBindAsDSV();
        void UnBindAsSRV();
        void UnBindAsCBV();
        void UnBindAsVB();
        void UnBindAsIB();
        float m_MinLOD;

        ResourceCreationArgs m_creationArgs;

        struct STilePoolAllocation
        {
            // For tier 1, attributed heaps need to be used
            // For tier 2, only the buffer heap is used
            unique_comptr<ID3D12Heap> m_spUnderlyingBufferHeap;
            unique_comptr<ID3D12Heap> m_spUnderlyingTextureHeap;
            UINT m_Size;
            UINT m_TileOffset;

            STilePoolAllocation(UINT size, UINT offset) : m_Size(size), m_TileOffset(offset) { }
            STilePoolAllocation() : m_Size(0), m_TileOffset(0) { }
            STilePoolAllocation(STilePoolAllocation&& other)
                : m_spUnderlyingBufferHeap(std::move(other.m_spUnderlyingBufferHeap))
                , m_spUnderlyingTextureHeap(std::move(other.m_spUnderlyingTextureHeap))
                , m_Size(std::move(other.m_Size))
                , m_TileOffset(std::move(other.m_TileOffset))
            { }
        };
        struct STilePoolData
        {
            std::vector<STilePoolAllocation> m_Allocations;
        };
        struct STiledResourceData
        {
            STiledResourceData(UINT NumSubresources, void*& pPreallocatedMemory)
                : m_SubresourceTiling(NumSubresources, pPreallocatedMemory)
            {
            }

            Resource* m_pTilePool = nullptr;
            PreallocatedArray<D3D12_SUBRESOURCE_TILING> m_SubresourceTiling;
            UINT m_NumStandardMips = 0;
            UINT m_NumTilesForResource = 0;
            UINT m_NumTilesForPackedMips = 0;
        };

        enum class EmulatedFormatMapState { Write, ReadWrite, Read, None };

        struct SEmulatedFormatSubresourceStagingAllocation
        {
            struct Deallocator
            {
                void operator()(void* pData) { AlignedHeapFree16(pData); }
            };
            std::unique_ptr<BYTE, Deallocator> m_pInterleavedData;
        };

        struct SEmulatedFormatSubresourceStagingData
        {
            EmulatedFormatMapState m_MapState = EmulatedFormatMapState::None;
            UINT m_MapRefCount = 0;
        };

        // Note: Must be declared before all members which have arrays sized by subresource index
        // For texture formats with both depth and stencil (D24S8 and D32S8X24),
        // D3D11 treats the depth and stencil as a single interleaved subresource,
        // while D3D12 treats them as independent planes, and therefore separate subresources
        // This is used on both default and staging textures with these formats
        // to modify subresource indices used for copies, transitions, and layout tracking
        const UINT8 m_SubresourceMultiplier;

        // All resources
        std::unique_ptr<SResourceIdentity> m_Identity;

        CResourceBindings m_currentBindings;

        UINT m_SRVUniqueness; // MinLOD, renaming
        UINT m_AllUniqueness; // Rotate

        // Internally used for indexing into arrays of data for dynamic
        // textures. Because textures with non-opaque planes share
        // an upload/readback heap, all non-opaque planes of the same 
        // mip+arrrayslice will have the same DynamicTextureIndex
        UINT GetDynamicTextureIndex(UINT Subresource)
        {
            UINT MipIndex, PlaneIndex, ArrayIndex;
            DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

            // NonOpaquePlanes share the same upload heap
            PlaneIndex = 0;
            return GetSubresourceIndex(PlaneIndex, MipIndex, ArrayIndex);
        }

        Resource* GetCurrentCpuHeap(UINT Subresource);

        void SetCurrentCpuHeap(UINT subresource, Resource* UploadHeap);

        void SetLastCopyCommandListID(UINT subresource, UINT64 commandListID)
        {
            UINT DynamicTextureIndex = GetDynamicTextureIndex(subresource);
            m_LastCommandListID[DynamicTextureIndex] = commandListID;
        }

        UINT64 GetLastCopyCommandListID(UINT subresource)
        {
            UINT DynamicTextureIndex = GetDynamicTextureIndex(subresource);
            return m_LastCommandListID[DynamicTextureIndex];
        }

        struct CpuHeapData
        {
            D3D12ResourceSuballocation m_subAllocation;
            UINT64 m_LastCopyCommandListID = 0;
        };

        // Dynamic textures:
        PreallocatedArray< unique_comptr<Resource> > m_spCurrentCpuHeaps;

        // Dynamic/staging textures:
        PreallocatedArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> m_SubresourcePlacement;

        // Staging textures:
        PreallocatedArray<SEmulatedFormatSubresourceStagingAllocation> m_FormatEmulationStagingAllocation;

        auto &GetFormatEmulationSubresourceStagingAllocation(UINT Subresource)
        {
            return m_FormatEmulationStagingAllocation[GetDynamicTextureIndex(Subresource)].m_pInterleavedData;
        }

        PreallocatedArray<SEmulatedFormatSubresourceStagingData> m_FormatEmulationStagingData;

        PreallocatedArray<UINT64> m_LastCommandListID;

        // For streamoutput buffers, 11on12 will add some bytes to the end
        // to hold a SStreamOutputSuffix
        // This contains the byte offset to that structure
        UINT m_OffsetToStreamOutputSuffix;

        // Tiled resources
        STilePoolData m_TilePool;
        STiledResourceData m_TiledResource;

        // The effective usage of the resource.  Row-major default textures are
        // treated like staging textures, because D3D12 doesn't support row-major
        // except for cross-adapter.
        RESOURCE_USAGE m_effectiveUsage;

        // The following are used to track state of mapped dynamic textures.  When
        // these textures are planar, each plane is mapped independently.  However
        // the same upload buffer must be used so that they are adjacent in memory;
        // This is required when all planes are mapped by an application 
        // using a single API call, even though the runtime splits it into three calls
        // to Map and Unmap.
        struct DynamicTexturePlaneData
        {
            UINT8 m_MappedPlaneRefCount[3] = {};
            UINT8 m_DirtyPlaneMask = 0;

            bool AnyPlaneMapped() const { return (*reinterpret_cast<const UINT32*>(this) & 0xffffff) != 0; }
        };

        PreallocatedArray<DynamicTexturePlaneData> m_DynamicTexturePlaneData;
        DynamicTexturePlaneData &GetDynamicTextureData(UINT subresource)
        {
            UINT DynamicTextureIndex = GetDynamicTextureIndex(subresource);
            return m_DynamicTexturePlaneData[DynamicTextureIndex];
        }

        bool m_isValid = false;

        // Fence used to ensure residency operations queued as part of UnwrapUnderlyingResource
        // operations are completed if the caller returns a resource without scheduling any work.
        DeferredWait m_UnwrapUnderlyingResidencyDeferredWait;

public:
        HRESULT AddFenceForUnwrapResidency(ID3D12CommandQueue* pQueue);

    };
};