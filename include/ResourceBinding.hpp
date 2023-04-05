// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Resource state tracking structures
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //----------------------------------------------------------------------------------------------------------------------------------
    // Tracks state due to bindings at a subresource granularity using bind refs
    class CSubresourceBindings
    {
    public:
        typedef void (CSubresourceBindings::*BindFunc)();

        CSubresourceBindings()
        {
        }

        bool IsBoundAsPixelShaderResource() const { return m_PSSRVBindRefs != 0; }
        bool IsBoundAsNonPixelShaderResource() const { return m_NonPSSRVBindRefs != 0; }
        bool IsBoundAsUnorderedAccess() const { return m_UAVBindRefs != 0; }
        bool IsBoundAsRenderTarget() const { return m_RTVBindRefs != 0; }
        bool IsBoundAsWritableDepth() const { return m_bIsDepthOrStencilBoundForWrite; }
        bool IsBoundAsReadOnlyDepth() const { return m_bIsDepthOrStencilBoundForReadOnly; }

        void PixelShaderResourceViewBound() { ++m_PSSRVBindRefs; }
        void NonPixelShaderResourceViewBound() { ++m_NonPSSRVBindRefs; }
        void UnorderedAccessViewBound() { ++m_UAVBindRefs; }
        void RenderTargetViewBound() { ++m_RTVBindRefs; }
        void ReadOnlyDepthStencilViewBound() { m_bIsDepthOrStencilBoundForReadOnly = true; }
        void WritableDepthStencilViewBound() { m_bIsDepthOrStencilBoundForWrite = true; }

        void PixelShaderResourceViewUnbound() { --m_PSSRVBindRefs; }
        void NonPixelShaderResourceViewUnbound() { --m_NonPSSRVBindRefs; }
        void UnorderedAccessViewUnbound() { --m_UAVBindRefs; }
        void RenderTargetViewUnbound() { --m_RTVBindRefs; }
        void DepthStencilViewUnbound()
        {
            m_bIsDepthOrStencilBoundForReadOnly = false;
            m_bIsDepthOrStencilBoundForWrite = false;
        }

    private:
        union
        {
            UINT BindRefsUint = 0;
            struct
            {
                UINT m_PSSRVBindRefs : 8;
                UINT m_NonPSSRVBindRefs : 8;
                UINT m_UAVBindRefs : 7;
                UINT m_RTVBindRefs : 4;
                UINT m_bIsDepthOrStencilBoundForReadOnly : 1;
                UINT m_bIsDepthOrStencilBoundForWrite : 1;
            };
        };
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    // Tracks currently bound views, and buffer bind points
    template<typename TIface> class View;

    class CResourceBindings
    {
    public:
        CResourceBindings(UINT SubresourceCount, UINT BindFlags, void*& pPreallocatedArray) noexcept;
        ~CResourceBindings();

        bool IsBoundAsShaderResource() const { return !D3D12TranslationLayer::IsListEmpty(&m_ShaderResourceViewList); }
        bool IsBoundAsRenderTarget() const { return !D3D12TranslationLayer::IsListEmpty(&m_RenderTargetViewList); }
        bool IsBoundAsUnorderedAccess() const { return !D3D12TranslationLayer::IsListEmpty(&m_UnorderedAccessViewList); }
        bool IsBoundAsDepthStencil() const { return m_bIsDepthStencilViewBound; }
        bool IsBoundAsConstantBuffer() const { return m_ConstantBufferBindRefs > 0; }
        bool IsBoundAsIndexBuffer() const { return m_bIsIndexBufferBound; }
        bool IsBoundAsVertexBuffer() const { return m_VertexBufferBindings != 0; }
        bool IsBoundAsStreamOut() const { return m_StreamOutBindings != 0; }

        void ViewBoundCommon(CViewSubresourceSubset& viewSubresources, CSubresourceBindings::BindFunc pfnBound);
        void ViewUnboundCommon(CViewSubresourceSubset& viewSubresources, CSubresourceBindings::BindFunc pfnUnbound);

        template<typename TIface> static CSubresourceBindings::BindFunc GetBindFunc(EShaderStage stage);
        template<typename TIface> static CSubresourceBindings::BindFunc GetUnbindFunc(EShaderStage stage);
        template<typename TIface> LIST_ENTRY& GetViewList();
        template<typename TIface> void ViewBound(View<TIface>* pView, EShaderStage stage, UINT slot);
        template<typename TIface> void ViewUnbound(View<TIface>* pView, EShaderStage stage, UINT slot);

        void VertexBufferBound(UINT slot);
        void StreamOutputBufferBound(UINT slot);
        void ConstantBufferBound(EShaderStage stage, UINT slot);
        void IndexBufferBound();

        void VertexBufferUnbound(UINT slot);
        void StreamOutputBufferUnbound(UINT slot);
        void ConstantBufferUnbound(EShaderStage stage, UINT slot);
        void IndexBufferUnbound();

        bool AreAllSubresourcesTheSame() const { return m_NumViewsReferencingSubresources == 0; }
        D3D12_RESOURCE_STATES GetD3D12ResourceUsageFromBindings(UINT subresource) const;
        COMMAND_LIST_TYPE GetCommandListTypeFromBindings() const;

        LIST_ENTRY& GetRenderTargetList() { return m_RenderTargetViewList; }
        LIST_ENTRY& GetShaderResourceList() { return m_ShaderResourceViewList; }
        UINT& GetVertexBufferBindings() { return m_VertexBufferBindings; }

        static size_t CalcPreallocationSize(UINT SubresourceCount) { return sizeof(CSubresourceBindings) * SubresourceCount; }

    private:
        friend class ImmediateContext;
        friend class Resource;
        const UINT m_BindFlags;
        PreallocatedArray<CSubresourceBindings> m_SubresourceBindings;
        UINT m_NumViewsReferencingSubresources;

        LIST_ENTRY m_ShaderResourceViewList;
        LIST_ENTRY m_RenderTargetViewList;
        LIST_ENTRY m_UnorderedAccessViewList;
        std::bitset<D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT> m_ConstantBufferBindings[ShaderStageCount];
        UINT m_ConstantBufferBindRefs = 0;
        UINT m_VertexBufferBindings; // bitfield
        UINT m_StreamOutBindings : 4; // bitfield
        bool m_bIsDepthStencilViewBound;
        bool m_bIsIndexBufferBound;
    };

    struct VBBinder
    {
        static void Bound(Resource* pBuffer, UINT slot, EShaderStage stage);
        static void Unbound(Resource* pBuffer, UINT slot, EShaderStage stage);
    };
    struct IBBinder
    {
        static void Bound(Resource* pBuffer, UINT slot, EShaderStage stage);
        static void Unbound(Resource* pBuffer, UINT slot, EShaderStage stage);
    };
    struct SOBinder
    {
        static void Bound(Resource* pBuffer, UINT slot, EShaderStage stage);
        static void Unbound(Resource* pBuffer, UINT slot, EShaderStage stage);
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    // Binding helpers
    // Tracks dirty bits, calls Bound/Unbound functions on binding changes,
    // and tracks binding data from shader decls to allow binding typed/additional NULLs
    //----------------------------------------------------------------------------------------------------------------------------------
    // Base class
    template <typename TBindable, UINT NumBindSlots>
    class CBoundState
    {
    public:
        static const UINT NumBindings = NumBindSlots;

    public:
        CBoundState() = default;

        bool DirtyBitsUpTo(_In_range_(0, NumBindings) UINT slot) const noexcept;
        void SetDirtyBit(_In_range_(0, NumBindings - 1) UINT slot) noexcept { m_DirtyBits.set(slot); }
        void SetDirtyBits(std::bitset<NumBindSlots> const& bits) noexcept { m_DirtyBits |= bits; }

        TBindable* const* GetBound() const noexcept { return m_Bound; }
        void ResetDirty(UINT slot) noexcept { m_DirtyBits.set(slot, false); }

        _Ret_range_(0, NumBindings) UINT GetNumBound() const noexcept { return m_NumBound; }
        void ReassertResourceState() const noexcept;
        
        bool UpdateBinding(_In_range_(0, NumBindings - 1) UINT slot, _In_opt_ TBindable* pBindable) noexcept
        {
            auto& Current = m_Bound[slot];
            if (pBindable)
            {
                m_NumBound = max(m_NumBound, slot + 1);
            }
            if (Current != pBindable)
            {
                Current = pBindable;
                if (!pBindable)
                {
                    TrimNumBound();
                }
                m_DirtyBits.set(slot);
                return true;
            }
            return false;
        }

        void Clear()
        {
            for (UINT i = 0; i < m_NumBound; ++i)
            {
                UpdateBinding(i, nullptr);
            }
        }

    protected:
        void TrimNumBound()
        {
            while (m_NumBound > 0 && !m_Bound[m_NumBound - 1])
            {
                --m_NumBound;
            }
        }

    protected:
        TBindable* m_Bound[NumBindings] = {};
        std::bitset<NumBindings> m_DirtyBits;
        _Field_range_(0, NumBindings) UINT m_NumBound = 0;
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    // Non-shader-visible (RTV, DSV, VB, IB, SO)
    template <typename TBindable, UINT NumBindSlots, typename TBinder = typename TBindable::TBinder>
    class CSimpleBoundState : public CBoundState<TBindable, NumBindSlots>
    {
    public:
        CSimpleBoundState() = default;

        bool UpdateBinding(_In_range_(0, NumBindings - 1) UINT slot, _In_opt_ TBindable* pBindable, EShaderStage stage) noexcept;
        bool IsDirty() const
        {
            return this->m_DirtyBits.any();
        }
        void ResetDirty()
        {
            this->m_DirtyBits.reset();
        }

        void Clear(EShaderStage shader)
        {
            for (UINT i = 0; i < this->m_NumBound; ++i)
            {
                UpdateBinding(i, nullptr, shader);
            }
        }
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    // SRV, UAV
    template <typename TBindable, UINT NumBindSlots>
    class CViewBoundState : public CBoundState<TBindable, NumBindSlots>
    {
    public:
        typedef TDeclVector::value_type NullType;
        typedef D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;
        static const NullType c_AnyNull = RESOURCE_DIMENSION::UNKNOWN;

    public:
        CViewBoundState() noexcept(false)
            : CBoundState<TBindable, NumBindSlots>()
        {
            m_ShaderData.reserve(NumBindings); // throw( bad_alloc )
        }

        bool UpdateBinding(_In_range_(0, NumBindings - 1) UINT slot, _In_opt_ TBindable* pBindable, EShaderStage stage) noexcept;
        bool IsDirty(TDeclVector const& New, UINT rootSignatureBucketSize, bool bKnownDirty) noexcept;

        NullType GetNullType(_In_range_(0, NumBindings - 1) UINT slot) const noexcept
        {
            if (slot >= m_ShaderData.size())
                return c_AnyNull;
            return m_ShaderData[slot];
        }

        void FillDescriptors(_Out_writes_(NumBindings) Descriptor* pDescriptors,
            _In_reads_(D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY) Descriptor* pNullDescriptors,
            _In_range_(0, NumBindings) UINT RootSignatureHWM) noexcept
        {
            for (UINT i = 0; i < RootSignatureHWM; ++i)
            {
                if (this->m_Bound[i])
                {
                    pDescriptors[i] = this->m_Bound[i]->GetRefreshedDescriptorHandle();
                }
                else
                {
                    pDescriptors[i] = pNullDescriptors[(UINT)GetNullType(i)];
                }
                this->m_DirtyBits.set(i, false);
            }
        }

        void Clear(EShaderStage shader)
        {
            for (UINT i = 0; i < NumBindSlots; ++i)
            {
                UpdateBinding(i, nullptr, shader);
            }
        }

    protected:
        TDeclVector m_ShaderData;
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    class CConstantBufferBoundState : public CBoundState<Resource, D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT>
    {
    public:
        CConstantBufferBoundState() noexcept
            : CBoundState()
        {
        }

        bool UpdateBinding(_In_range_(0, NumBindings - 1) UINT slot, _In_opt_ Resource* pBindable, EShaderStage stage) noexcept;

        bool IsDirty(_In_range_(0, NumBindings) UINT rootSignatureBucketSize) noexcept
        {
            bool bDirty = rootSignatureBucketSize > m_ShaderData || DirtyBitsUpTo(rootSignatureBucketSize);
            m_ShaderData = rootSignatureBucketSize;

            return bDirty;
        }

        void Clear(EShaderStage shader)
        {
            for (UINT i = 0; i < NumBindings; ++i)
            {
                UpdateBinding(i, nullptr, shader);
            }
        }
    protected:
        _Field_range_(0, NumBindings) UINT m_ShaderData = 0;
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    class CSamplerBoundState : public CBoundState<Sampler, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>
    {
    public:
        typedef D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;

    public:
        CSamplerBoundState() noexcept
            : CBoundState()
        {
        }

        bool UpdateBinding(_In_range_(0, NumBindings - 1) UINT slot,
            _In_ Sampler* pBindable) noexcept;

        bool IsDirty(_In_range_(0, NumBindings) UINT rootSignatureBucketSize) noexcept
        {
            bool bDirty = rootSignatureBucketSize > m_ShaderData || DirtyBitsUpTo(rootSignatureBucketSize);
            m_ShaderData = rootSignatureBucketSize;

            return bDirty;
        }

        void FillDescriptors(_Out_writes_(NumBindings) Descriptor* pDescriptors, Descriptor* pNullDescriptor,
            _In_range_(0, NumBindings) UINT RootSignatureHWM) noexcept
        {
            for (UINT i = 0; i < RootSignatureHWM; ++i)
            {
                pDescriptors[i] = (m_Bound[i]) ? m_Bound[i]->m_Descriptor : *pNullDescriptor;
                m_DirtyBits.set(i, false);
            }
        }

        void Clear()
        {
            for (UINT i = 0; i < NumBindings; ++i)
            {
                UpdateBinding(i, nullptr);
            }
        }

    protected:
        _Field_range_(0, NumBindings) UINT m_ShaderData = 0;
    };

};
