// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{
    //==================================================================================================================================
    // CResourceBindings
    // The inverse of CContext::m_PState, allows fast lookups for bind points of a given resource
    // Used to determine resource/subresource state, as well as for rebinding resources on discard/LOD change
    //==================================================================================================================================

    CResourceBindings::CResourceBindings(UINT SubresourceCount, UINT BindFlags, void*& pPreallocatedArray) noexcept
        : m_BindFlags(BindFlags)
        , m_NumViewsReferencingSubresources(0)
        , m_VertexBufferBindings(0)
        , m_StreamOutBindings(0)
        , m_bIsDepthStencilViewBound(false)
        , m_bIsIndexBufferBound(false)
        , m_SubresourceBindings(SubresourceCount, pPreallocatedArray)
    {
        D3D12TranslationLayer::InitializeListHead(&m_ShaderResourceViewList);
        D3D12TranslationLayer::InitializeListHead(&m_RenderTargetViewList);
        D3D12TranslationLayer::InitializeListHead(&m_UnorderedAccessViewList);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    CResourceBindings::~CResourceBindings()
    {
        assert(D3D12TranslationLayer::IsListEmpty(&m_ShaderResourceViewList));
        assert(D3D12TranslationLayer::IsListEmpty(&m_RenderTargetViewList));
        assert(D3D12TranslationLayer::IsListEmpty(&m_UnorderedAccessViewList));
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::ViewBoundCommon(CViewSubresourceSubset& viewSubresources, void (CSubresourceBindings::*pfnBound)())
    {
        if (!viewSubresources.IsWholeResource())
        {
            ++m_NumViewsReferencingSubresources;
        }
        for (auto subresources : viewSubresources)
        {
            for (UINT i = subresources.first; i < subresources.second; ++i)
            {
                (m_SubresourceBindings[i].*pfnBound)();
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::ViewUnboundCommon(CViewSubresourceSubset& viewSubresources, void (CSubresourceBindings::*pfnUnbound)())
    {
        if (!viewSubresources.IsWholeResource())
        {
            --m_NumViewsReferencingSubresources;
        }
        for (auto subresources : viewSubresources)
        {
            for (UINT i = subresources.first; i < subresources.second; ++i)
            {
                (m_SubresourceBindings[i].*pfnUnbound)();
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::VertexBufferBound(UINT slot)
    {
        assert(slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
        UINT slotBit = (1 << slot);
        assert((m_VertexBufferBindings & slotBit) == 0);
        m_VertexBufferBindings |= slotBit;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::StreamOutputBufferBound(UINT slot)
    {
        assert(slot < D3D11_SO_STREAM_COUNT);
        UINT slotBit = (1 << slot);
        assert((m_StreamOutBindings & slotBit) == 0);
        m_StreamOutBindings |= slotBit;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::ConstantBufferBound(EShaderStage stage, UINT slot)
    {
        m_ConstantBufferBindings[stage].set(slot);
        m_ConstantBufferBindRefs++;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::IndexBufferBound()
    {
        assert(!m_bIsIndexBufferBound);
        m_bIsIndexBufferBound = true;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::VertexBufferUnbound(UINT slot)
    {
        assert(slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
        UINT slotBit = (1 << slot);
        assert((m_VertexBufferBindings & slotBit) == slotBit);
        m_VertexBufferBindings &= ~slotBit;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::StreamOutputBufferUnbound(UINT slot)
    {
        assert(slot < D3D11_SO_STREAM_COUNT);
        UINT slotBit = (1 << slot);
        assert((m_StreamOutBindings & slotBit) == slotBit);
        m_StreamOutBindings &= ~slotBit;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::ConstantBufferUnbound(EShaderStage stage, UINT slot)
    {
        assert(m_ConstantBufferBindRefs > 0 && m_ConstantBufferBindings[stage][slot]);
        m_ConstantBufferBindRefs--;
        m_ConstantBufferBindings[stage].set(slot, false);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CResourceBindings::IndexBufferUnbound()
    {
        assert(m_bIsIndexBufferBound);
        m_bIsIndexBufferBound = false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    D3D12_RESOURCE_STATES CResourceBindings::GetD3D12ResourceUsageFromBindings(UINT subresource) const
    {
        const CSubresourceBindings& S = m_SubresourceBindings[subresource];
        D3D12_RESOURCE_STATES BufferUsage = D3D12_RESOURCE_STATE_COMMON;
        if (subresource == 0 && m_SubresourceBindings.size() == 1)
        {
            BufferUsage = (IsBoundAsVertexBuffer() || IsBoundAsConstantBuffer() ? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : D3D12_RESOURCE_STATE_COMMON) |
                (IsBoundAsIndexBuffer() ? D3D12_RESOURCE_STATE_INDEX_BUFFER : D3D12_RESOURCE_STATE_COMMON) |
                (IsBoundAsStreamOut() ? D3D12_RESOURCE_STATE_STREAM_OUT : D3D12_RESOURCE_STATE_COMMON);

            // If a buffer has the STREAM_OUTPUT bind flag
            // and is bound as a VB, then add the IndirectArgument resource usage
            // This is needed because DrawAuto is converted to DrawIndirect
            // and the indirect arguments are at the end of the buffer
            if (IsBoundAsVertexBuffer() && (m_BindFlags & D3D11_BIND_STREAM_OUTPUT))
            {
                BufferUsage |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            }
        }
        assert(!(S.IsBoundAsWritableDepth() && S.IsBoundAsReadOnlyDepth()));
        D3D12_RESOURCE_STATES Usage = BufferUsage |
            (S.IsBoundAsPixelShaderResource() ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON) |
            (S.IsBoundAsNonPixelShaderResource() ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON) |
            (S.IsBoundAsRenderTarget() ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COMMON) |
            (S.IsBoundAsUnorderedAccess() ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COMMON) |
            (S.IsBoundAsWritableDepth() ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COMMON) |
            (S.IsBoundAsReadOnlyDepth() ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);
        return Usage ? Usage : UNKNOWN_RESOURCE_STATE;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    COMMAND_LIST_TYPE CResourceBindings::GetCommandListTypeFromBindings() const
    {
        if (m_BindFlags & D3D11_BIND_DECODER)
        {
            return COMMAND_LIST_TYPE::VIDEO_DECODE;
        }
        else
        {
            return COMMAND_LIST_TYPE::GRAPHICS;
        }
    }
};