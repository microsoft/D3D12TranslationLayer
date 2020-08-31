// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class Resource;
    //==================================================================================================================================
    // View
    // Stores data responsible for remapping D3D11 views to underlying D3D12 views
    //==================================================================================================================================

    template <UINT NumStages, UINT NumBindPoints>
    class CViewBindingsImpl
    {
    public:
        CViewBindingsImpl() { D3D12TranslationLayer::InitializeListHead(&m_ViewBindingList); }
        ~CViewBindingsImpl() { if (IsViewBound()) { D3D12TranslationLayer::RemoveEntryList(&m_ViewBindingList); } }

        bool IsViewBound() { return !D3D12TranslationLayer::IsListEmpty(&m_ViewBindingList); }

        void ViewBound(UINT stage, UINT slot) { m_BindPoints[stage].set(slot); }
        void ViewUnbound(UINT stage, UINT slot) { assert(m_BindPoints[stage][slot]); m_BindPoints[stage].set(slot, false); }

    private:
        friend class CResourceBindings;
        friend class ImmediateContext;
        friend class Resource;

        std::bitset<NumBindPoints> m_BindPoints[NumStages];
        LIST_ENTRY m_ViewBindingList;
    };

    // These types are purely used to specialize the templated
    // view class
    enum class ShaderResourceViewType {};
    enum class RenderTargetViewType {};
    enum class DepthStencilViewType {};
    enum class UnorderedAccessViewType {};
    enum class VideoDecoderOutputViewType {};
    enum class VideoProcessorInputViewType {};
    enum class VideoProcessorOutputViewType {};

    template< class TIface >
    struct CViewMapper;

    struct D3D12_UNORDERED_ACCESS_VIEW_DESC_WRAPPER
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC m_Desc12;
        UINT m_D3D11UAVFlags;
    };

#define DECLARE_VIEW_MAPPER(View, DescType12, TranslationLayerDesc) \
    template<> struct CViewMapper<##View##Type> \
    { \
    typedef TranslationLayerDesc TTranslationLayerDesc; \
    typedef D3D12_##DescType12 TDesc12; \
    static decltype(&ID3D12Device::Create##View) GetCreate() { return &ID3D12Device::Create##View; } \
    }

#define DECLARE_VIEW_MAPPER1(View, DescType, TranslationLayerDesc) \
    template<> struct CViewMapper<##View##Type> \
    { \
    typedef TranslationLayerDesc TTranslationLayerDesc; \
    typedef DescType TDesc12; \
    }

    DECLARE_VIEW_MAPPER(ShaderResourceView, SHADER_RESOURCE_VIEW_DESC, D3D12_SHADER_RESOURCE_VIEW_DESC);
    DECLARE_VIEW_MAPPER(RenderTargetView, RENDER_TARGET_VIEW_DESC, D3D12_RENDER_TARGET_VIEW_DESC);
    DECLARE_VIEW_MAPPER(DepthStencilView, DEPTH_STENCIL_VIEW_DESC, D3D12_DEPTH_STENCIL_VIEW_DESC);
    DECLARE_VIEW_MAPPER(UnorderedAccessView, UNORDERED_ACCESS_VIEW_DESC, D3D12_UNORDERED_ACCESS_VIEW_DESC_WRAPPER);
    DECLARE_VIEW_MAPPER1(VideoDecoderOutputView, VIDEO_DECODER_OUTPUT_VIEW_DESC_INTERNAL, VIDEO_DECODER_OUTPUT_VIEW_DESC_INTERNAL);
    DECLARE_VIEW_MAPPER1(VideoProcessorInputView, VIDEO_PROCESSOR_INPUT_VIEW_DESC_INTERNAL, VIDEO_PROCESSOR_INPUT_VIEW_DESC_INTERNAL);
    DECLARE_VIEW_MAPPER1(VideoProcessorOutputView, VIDEO_PROCESSOR_OUTPUT_VIEW_DESC_INTERNAL, VIDEO_PROCESSOR_OUTPUT_VIEW_DESC_INTERNAL);
#undef DECLARE_VIEW_MAPPER

    template <class TIface> struct CViewBindingsMapper { using Type = CViewBindingsImpl<1, 1>; };
    template <> struct CViewBindingsMapper<ShaderResourceViewType> { using Type = CViewBindingsImpl<ShaderStageCount, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>; };
    template <> struct CViewBindingsMapper<RenderTargetViewType> { using Type = CViewBindingsImpl<1, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>; };
    template <> struct CViewBindingsMapper<UnorderedAccessViewType> { using Type = CViewBindingsImpl<UAVStageCount, D3D11_1_UAV_SLOT_COUNT>; };
    template <class TIface> using CViewBindings = typename CViewBindingsMapper<TIface>::Type;

    class ViewBase : public DeviceChild
    {
    public: // Methods
        ViewBase(ImmediateContext* pDevice, Resource* pResource, CViewSubresourceSubset const& Subresources) noexcept;

        // Note: This is hiding the base class implementation not overriding it
        // Warning: this method is hidden in the UAV type, and is not virtual
        // Always ensure that this method is called on the most derived type.
        void UsedInCommandList(COMMAND_LIST_TYPE commandListType, UINT64 id);

    public: // Members
        Resource* const m_pResource;

    protected:
        D3D12_CPU_DESCRIPTOR_HANDLE m_Descriptor;
        UINT m_DescriptorHeapIndex;

    public:
        CViewSubresourceSubset m_subresources;
        UINT m_ViewUniqueness;
    };

    template< class TIface >
    class View : public ViewBase
    {
    public: // Types
        typedef CViewMapper<TIface> TMapper;
        typedef typename CViewMapper<TIface>::TDesc12 TDesc12;
        typedef typename CViewMapper<TIface>::TTranslationLayerDesc TTranslationLayerDesc;

        struct TBinder
        {
            static void Bound(View<TIface>* pView, UINT slot, EShaderStage stage) { if (pView) pView->ViewBound(slot, stage); }
            static void Unbound(View<TIface>* pView, UINT slot, EShaderStage stage) { if (pView) pView->ViewUnbound(slot, stage); }
        };

    public: // Methods
        static View *CreateView(ImmediateContext* pDevice, const typename TDesc12 &Desc, Resource &ViewResource) noexcept(false) { return new View(pDevice, Desc, ViewResource); }
        static void DestroyView(View* pView) { delete pView; }

        View(ImmediateContext* pDevice, const typename TDesc12 &Desc, Resource &ViewResource) noexcept(false);
        ~View() noexcept;

        const TDesc12& GetDesc12() noexcept;

        bool IsUpToDate() const noexcept { return m_pResource->GetUniqueness<TIface>() == m_ViewUniqueness; }
        HRESULT RefreshUnderlying() noexcept;
        D3D12_CPU_DESCRIPTOR_HANDLE GetRefreshedDescriptorHandle()
        {
            HRESULT hr = RefreshUnderlying();
            if (FAILED(hr))
            {
                assert(hr != E_INVALIDARG);
                ThrowFailure(hr);
            }
            return m_Descriptor;
        }

        void ViewBound(UINT Slot = 0, EShaderStage = e_PS) noexcept;
        void ViewUnbound(UINT Slot = 0, EShaderStage = e_PS) noexcept;

        UINT16 GetBindRefs() { return m_BindRefs; }
        void IncrementBindRefs() { m_BindRefs++; }
        void DecrementBindRefs() 
        {
            assert(m_BindRefs > 0);
            m_BindRefs--; 
        }

    public:
        CViewBindings<TIface> m_currentBindings;

    private:
        UINT16 m_BindRefs;
        TDesc12 m_Desc;

        // We tamper with m_Desc.Buffer.FirstElement when renaming resources for map discard so it is important that we record the 
        // original first element expected by the API
        UINT64 APIFirstElement;

        void UpdateMinLOD(float MinLOD);
    };

    typedef View<ShaderResourceViewType> TSRV;
    typedef View<RenderTargetViewType> TRTV;
    typedef View<DepthStencilViewType> TDSV;
    typedef View<UnorderedAccessViewType> TUAV;
    typedef View<VideoDecoderOutputViewType> TVDOV;
    typedef View<VideoProcessorInputViewType> TVPIV;
    typedef View<VideoProcessorOutputViewType> TVPOV;

    // Counter and Append UAVs have an additional resource allocated
    // to hold the counter value
    class UAV : public TUAV
    {
    public:
        UAV(ImmediateContext* pDevice, const TTranslationLayerDesc &Desc, Resource &ViewResource) noexcept(false);
        ~UAV() noexcept(false);

        //Note: This is hiding the base class implementation not overriding it
        void UsedInCommandList(COMMAND_LIST_TYPE commandListType, UINT64 id)
        {
            TUAV::UsedInCommandList(commandListType, id);
            DeviceChild::UsedInCommandList(commandListType, id);
        }

        static UAV *CreateView(ImmediateContext* pDevice, const TTranslationLayerDesc &Desc, Resource &ViewResource) noexcept(false) { return new UAV(pDevice, Desc, ViewResource); }

        void EnsureCounterResource() noexcept(false);

        void UpdateCounterValue(UINT Value);

        void CopyCounterToBuffer(ID3D12Resource* pDst, UINT DstOffset) noexcept;

    public:
        UINT m_D3D11UAVFlags;
        D3D12TranslationLayer::unique_comptr<ID3D12Resource> m_pCounterResource;
    };

    class CDescriptorHeapManager;
    struct DescriptorHeapEntry
    {
        DescriptorHeapEntry(CDescriptorHeapManager *pDescriptorHeapManager, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor, UINT DescriptorHeapIndex, UINT64 LastUsedCommandListID) :
            m_pDescriptorHeapManager(pDescriptorHeapManager), m_Descriptor(Descriptor), m_DescriptorHeapIndex(DescriptorHeapIndex), m_LastUsedCommandListID(LastUsedCommandListID) {}

        D3D12_CPU_DESCRIPTOR_HANDLE m_Descriptor;
        CDescriptorHeapManager *m_pDescriptorHeapManager;
        UINT m_DescriptorHeapIndex;
        UINT64 m_LastUsedCommandListID;
    };

    typedef TSRV SRV;
    typedef TRTV RTV;
    typedef TDSV DSV;
    typedef TVDOV VDOV;
    typedef TVPIV VPIV;
    typedef TVPOV VPOV;
};