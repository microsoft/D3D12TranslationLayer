// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "BlitHelperShaders.h"

const UINT MAX_PLANES = 3;

namespace D3D12TranslationLayer
{
    static UINT RectHeight(const RECT& r)
    {
        return r.bottom - r.top;
    }

    static UINT RectWidth(const RECT& r)
    {
        return r.right - r.left;
    }

    BlitHelper::BlitHelper(ImmediateContext *pContext) :
        m_pParent(pContext)
    {
    }

    auto BlitHelper::PrepareShaders(Resource *pSrc, UINT srcPlanes, Resource *pDst, UINT dstPlanes, bool bEnableAlpha, bool bSwapRB, int& outSrcPixelScalingFactor) -> BlitPipelineState*
    {
        const D3D12_RESOURCE_DESC &srcDesc = pSrc->GetUnderlyingResource()->GetDesc();
        const D3D12_RESOURCE_DESC &dstDesc = pDst->GetUnderlyingResource()->GetDesc();

        if (CD3D11FormatHelper::YUV(dstDesc.Format))
        {
            // YUV -> RGB conversion only for now, or RGB -> RGB stretching
            throw _com_error(E_INVALIDARG);
        }

        if (srcPlanes > MAX_PLANES)
        {
            throw _com_error(E_INVALIDARG);
        }

        if (dstPlanes != 1)
        {
            // just RGB output for now
            throw _com_error(E_INVALIDARG);
        }

        BlitHelperKeyUnion key;
        key.m_Bits.SrcFormat = srcDesc.Format;
        key.m_Bits.DstFormat = dstDesc.Format;
        key.m_Bits.DstSampleCount = dstDesc.SampleDesc.Count;
        key.m_Bits.bSwapRB = bSwapRB;
        key.m_Bits.bEnableAlpha = bEnableAlpha;
        key.m_Bits.Unused = 0;
        auto& spPSO = m_spBlitPSOs[key.m_Data];
        if (!spPSO)
        {
            spPSO.reset(new BlitPipelineState(m_pParent));
            struct ConvertPSOStreamDescriptor
            {
                CD3DX12_PIPELINE_STATE_STREAM_VS VS{ CD3DX12_SHADER_BYTECODE(g_VSMain, sizeof(g_VSMain)) };
                CD3DX12_PIPELINE_STATE_STREAM_PS PS;
                CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
                CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
                CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
                CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DSS;
                CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
                CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC Samples;
                CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask{ UINT_MAX };
            } psoStream;
            
            outSrcPixelScalingFactor = 1;
            if (srcDesc.Format == DXGI_FORMAT_P010 ||
                srcDesc.Format == DXGI_FORMAT_Y210)
            {
                // we need to add additional math in the shader to scale from the 10bit range into the output [0,1]. As
                // the input to the shader is normalized, we need to multiply by 2^6 to get a float in the [0,1] range.
                outSrcPixelScalingFactor = 64;
            }

            switch (srcPlanes)
            {
            case 3:
                psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PS3PlaneYUV, sizeof(g_PS3PlaneYUV)); break;
            case 2:
                psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PS2PlaneYUV, sizeof(g_PS2PlaneYUV)); break;
            default:
                switch (srcDesc.Format)
                {
                case DXGI_FORMAT_AYUV:
                    psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PSAYUV, sizeof(g_PSAYUV)); break;
                case DXGI_FORMAT_Y410:
                case DXGI_FORMAT_Y416:
                    psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PSY4XX, sizeof(g_PSY4XX)); break;
                case DXGI_FORMAT_YUY2:
                case DXGI_FORMAT_Y210:
                case DXGI_FORMAT_Y216:
                    psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PSPackedYUV, sizeof(g_PSPackedYUV)); break;
                default:
                    if (bSwapRB)
                    {
                        psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PSBasic_SwapRB, sizeof(g_PSBasic_SwapRB));
                    }
                    else
                    {
                        psoStream.PS = CD3DX12_SHADER_BYTECODE(g_PSBasic, sizeof(g_PSBasic));
                    }
                }
                break;
            }
            psoStream.NodeMask = m_pParent->GetNodeMask();
            psoStream.RTVFormats = D3D12_RT_FORMAT_ARRAY{ { dstDesc.Format }, 1 };
            psoStream.Samples = dstDesc.SampleDesc;
            CD3DX12_DEPTH_STENCIL_DESC DSS(CD3DX12_DEFAULT{});
            DSS.DepthEnable = false;
            psoStream.DSS = DSS;
            if (bEnableAlpha)
            {
                auto& blendDesc = static_cast<CD3DX12_BLEND_DESC&>(psoStream.Blend).RenderTarget[0];
                blendDesc.BlendEnable = TRUE;
                blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
                blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
                blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            }
            D3D12_PIPELINE_STATE_STREAM_DESC psoStreamDesc = { sizeof(psoStream), &psoStream };
            ThrowFailure(m_pParent->m_pDevice12_2->CreatePipelineState(&psoStreamDesc, IID_PPV_ARGS(spPSO->GetForCreate())));
        }

        if (!m_spRootSig)
        {
            m_spRootSig.reset(new InternalRootSignature(m_pParent)); // throw( bad_alloc )
            m_spRootSig->Create(g_PSBasic, sizeof(g_PSBasic)); // throw( _com_error )
        }

        return spPSO.get();
    }

    void BlitHelper::Blit(Resource* pSrc, UINT SrcSubresourceIdx, const RECT& SrcRect, Resource* pDst, UINT DstSubresourceIdx, const RECT& DstRect, bool bEnableAlpha, bool bSwapRBChannels)
    {
        UINT SrcPlaneCount = pSrc->AppDesc()->NonOpaquePlaneCount();
        UINT DstPlaneCount = pDst->AppDesc()->NonOpaquePlaneCount();
        UINT SrcSubresourceIndices[MAX_PLANES];
        UINT DstSubresourceIndices[MAX_PLANES];
        auto FillIndices = [](Resource* pResource, UINT SubresourceIdx, UINT PlaneCount, UINT* pIndices)
        {
            UINT MipLevel, ArraySlice, PlaneIdx;
            pResource->DecomposeSubresource(SubresourceIdx, MipLevel, ArraySlice, PlaneIdx);
            assert(PlaneIdx == 0);
            for (UINT i = 0; i < PlaneCount; ++i)
            {
                pIndices[i] = pResource->GetSubresourceIndex(i, MipLevel, ArraySlice);
            }
        };
        FillIndices(pSrc, SrcSubresourceIdx, SrcPlaneCount, SrcSubresourceIndices);
        FillIndices(pDst, DstSubresourceIdx, DstPlaneCount, DstSubresourceIndices);
        Blit(pSrc, SrcSubresourceIndices, SrcPlaneCount, SrcRect, pDst, DstSubresourceIndices, DstPlaneCount, DstRect, bEnableAlpha, bSwapRBChannels);
    }

    void BlitHelper::Blit(Resource *pSrc, UINT *pSrcSubresourceIndices, UINT numSrcSubresources, const RECT& srcRect, Resource *pDst, UINT *pDstSubresourceIndices, UINT numDstSubresources, const RECT& dstRect, bool bEnableAlpha, bool bSwapRBChannels)
    {
        const D3D12_RESOURCE_DESC &dstDesc = pDst->GetUnderlyingResource()->GetDesc();
        assert( numSrcSubresources <= MAX_PLANES );
        UINT nonMsaaSrcSubresourceIndices[MAX_PLANES];
        memcpy( &nonMsaaSrcSubresourceIndices[0], pSrcSubresourceIndices, numSrcSubresources * sizeof(UINT) );
        ResourceCacheEntry OwnedCacheEntryFromResolve;
        auto srcFormat = pSrc->AppDesc()->Format();
        auto dstFormat = pDst->AppDesc()->Format();
        bool needsTempRenderTarget = (dstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_NONE;
        bool needsTwoPassColorConvert = (bSwapRBChannels && CD3D11FormatHelper::YUV( srcFormat ));

        //If the src is MSAA, resolve to non-MSAA
        if (pSrc->AppDesc()->Samples() > 1)
        {
            assert( !needsTwoPassColorConvert ); //Can't have MSAA YUV resources, so this should be false
            ResolveToNonMsaa( &pSrc /*inout*/, nonMsaaSrcSubresourceIndices /*inout*/, numSrcSubresources );

            // We used a Cache Entry of pSrc's format to do the resolve. 
            // If pDst uses the same format and we need a temp render target, 
            // we should take ownership of the resource used for resolve so that
            // a new resource can be used for the destination of the blit.
            // This is to prevent the same resource being used for read and write (see comment block below).
            if (srcFormat == dstFormat && needsTempRenderTarget)
            {
                m_pParent->GetResourceCache().TakeCacheEntryOwnership( srcFormat, OwnedCacheEntryFromResolve );
            }
        }

        int srcPixelScalingFactor = 1;
        BlitPipelineState* pPSO = PrepareShaders(pSrc, numSrcSubresources, pDst, numDstSubresources, bEnableAlpha, bSwapRBChannels, srcPixelScalingFactor /*out argument*/);

        m_pParent->PreRender(COMMAND_LIST_TYPE::GRAPHICS);

        //
        // setup the RTV
        //
        auto pNewDestinationResource = pDst;
        RTV* pRTV = nullptr;
        std::optional<RTV> LocalRTV;
        ResourceCacheEntry OwnedCacheEntry;
        if (needsTempRenderTarget || needsTwoPassColorConvert)
        {
            auto& CacheEntry = m_pParent->GetResourceCache().GetResource(dstFormat,
                RectWidth(dstRect),
                RectHeight(dstRect));
            pNewDestinationResource = CacheEntry.m_Resource.get();
            pRTV = CacheEntry.m_RTV.get();

            if (needsTempRenderTarget && needsTwoPassColorConvert)
            {
                // If we're doing both of these, then our two-pass approach is going to turn into
                // a three-pass approach (first color convert to a cache entry, then swap RB channels
                // into another cache entry, then copy to the non-renderable destination).
                // In this case, passes one and two will both try to render to the cache entry,
                // with pass two trying to simultaneously read AND write from the cache entry.
                //
                // To prevent this, take ownership of the cache entry during pass one, so that pass two
                // allocates a new resource. The resource from pass one will then be destroyed rather than
                // being cached... but this seems okay since this should be *very* uncommon
                m_pParent->GetResourceCache().TakeCacheEntryOwnership(dstFormat, OwnedCacheEntry);
            }
        }
        else
        {
            UINT subresource = pDstSubresourceIndices[0];
            UINT8 DstPlane = 0, DstMip = 0;
            UINT16 DstArraySlice = 0;
            D3D12DecomposeSubresource(subresource, pDst->AppDesc()->MipLevels(), pDst->AppDesc()->ArraySize(), DstMip, DstArraySlice, DstPlane);

            D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
            RTVDesc.Format = dstDesc.Format;
            if (pDst->AppDesc()->Samples() > 1)
            {
                RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                RTVDesc.Texture2DMSArray.ArraySize = 1;
                RTVDesc.Texture2DMSArray.FirstArraySlice = DstArraySlice;
            }
            else
            {
                RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                RTVDesc.Texture2DArray.MipSlice = DstMip;
                RTVDesc.Texture2DArray.PlaneSlice = DstPlane;
                RTVDesc.Texture2DArray.ArraySize = 1;
                RTVDesc.Texture2DArray.FirstArraySlice = DstArraySlice;
            }
            LocalRTV.emplace(m_pParent, RTVDesc, *pDst);
            pRTV = &LocalRTV.value();
        }

        //
        // Transition the src & dst resources
        //
        {
            for (UINT i = 0; i < numSrcSubresources; i++)
            {
                UINT subresourceIndex = nonMsaaSrcSubresourceIndices[i];
                m_pParent->GetResourceStateManager().TransitionSubresource(pSrc, subresourceIndex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            assert(numDstSubresources == 1);        // for now, just packed output
            for (UINT i = 0; i < numDstSubresources; i++)
            {
                UINT subresourceIndex = needsTempRenderTarget ? 0 : pDstSubresourceIndices[i];
                m_pParent->GetResourceStateManager().TransitionSubresource(pNewDestinationResource, subresourceIndex, D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
            m_pParent->GetResourceStateManager().ApplyAllResourceTransitions();
        }

        // No predication in DX9
        ImmediateContext::CDisablePredication DisablePredication(m_pParent);

        UINT SRVBaseSlot = m_pParent->ReserveSlots(m_pParent->m_ViewHeap, MAX_PLANES);
        ID3D12GraphicsCommandList* pCommandList = m_pParent->GetGraphicsCommandList();
        pCommandList->SetGraphicsRootSignature(m_spRootSig->GetRootSignature());
        pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        {
            // Unbind all VBs
            D3D12_VERTEX_BUFFER_VIEW VBVArray[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            memset(VBVArray, 0, sizeof(VBVArray));
            pCommandList->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, VBVArray);
        }

        //
        // set up the SRVs
        //
        for (UINT i = 0; i < numSrcSubresources; i++)
        {
            UINT subresource = nonMsaaSrcSubresourceIndices[i];
            UINT8 SrcPlane = 0, SrcMip = 0;
            UINT16 SrcArraySlice = 0;
            D3D12DecomposeSubresource(subresource, pSrc->AppDesc()->MipLevels(), pSrc->AppDesc()->ArraySize(), SrcMip, SrcArraySlice, SrcPlane);
            auto& SrcFootprint = pSrc->GetSubresourcePlacement(subresource).Footprint;

            DXGI_FORMAT ViewFormat = SrcFootprint.Format;
            switch (ViewFormat)
            {
            case DXGI_FORMAT_R8_TYPELESS:
                ViewFormat = DXGI_FORMAT_R8_UNORM;
                break;
            case DXGI_FORMAT_R8G8_TYPELESS:
                ViewFormat = DXGI_FORMAT_R8G8_UNORM;
                break;
            case DXGI_FORMAT_R16_TYPELESS:
                ViewFormat = DXGI_FORMAT_R16_UNORM;
                break;
            case DXGI_FORMAT_R16G16_TYPELESS:
                ViewFormat = DXGI_FORMAT_R16G16_UNORM;
                break;
            case DXGI_FORMAT_AYUV: // 8bpc
            case DXGI_FORMAT_YUY2: // 8bpc
                ViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            case DXGI_FORMAT_Y410: // 10bpc except 2 bit alpha
                ViewFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
                break;
            case DXGI_FORMAT_Y416: // 16bpc
            case DXGI_FORMAT_Y210: // 10bpc for all 4 channels
            case DXGI_FORMAT_Y216: // 16bpc
                ViewFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
                break;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = ViewFormat;
            SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (pSrc->AppDesc()->Samples() > 1)
            {
                SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                SRVDesc.Texture2DMSArray.ArraySize = 1;
                SRVDesc.Texture2DMSArray.FirstArraySlice = SrcArraySlice;
            }
            else
            {
                SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                SRVDesc.Texture2DArray.MipLevels = 1;
                SRVDesc.Texture2DArray.MostDetailedMip = SrcMip;
                SRVDesc.Texture2DArray.PlaneSlice = SrcPlane;
                SRVDesc.Texture2DArray.ArraySize = 1;
                SRVDesc.Texture2DArray.FirstArraySlice = SrcArraySlice;
                SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE SRVBaseCPU = m_pParent->m_ViewHeap.CPUHandle(SRVBaseSlot + i);
            m_pParent->m_pDevice12->CreateShaderResourceView(pSrc->GetUnderlyingResource(), &SRVDesc, SRVBaseCPU);
        }
        for (UINT i = numSrcSubresources; i < MAX_PLANES; ++i)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE SRVBaseCPU = m_pParent->m_ViewHeap.CPUHandle(SRVBaseSlot + i);
            m_pParent->m_pDevice12->CopyDescriptorsSimple(1, SRVBaseCPU, m_pParent->m_NullSRVs[(UINT)RESOURCE_DIMENSION::TEXTURE2DARRAY], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        D3D12_GPU_DESCRIPTOR_HANDLE SRVBaseGPU = m_pParent->m_ViewHeap.GPUHandle(SRVBaseSlot);
        pCommandList->SetGraphicsRootDescriptorTable(0, SRVBaseGPU);

        pCommandList->OMSetRenderTargets(1, &pRTV->GetRefreshedDescriptorHandle(), TRUE, nullptr);

        // Constant buffers: srcRect, src dimensions
        {
            UINT subresourceIndex = nonMsaaSrcSubresourceIndices[0];
            auto& srcSubresourceFootprint = pSrc->GetSubresourcePlacement(subresourceIndex).Footprint;
            int srcPositions[6] = { srcRect.left, srcRect.right, srcRect.top, srcRect.bottom, (int)srcSubresourceFootprint.Width, (int)srcSubresourceFootprint.Height };

            if (srcSubresourceFootprint.Format == DXGI_FORMAT_YUY2 &&
                m_pParent->m_CreationArgs.AdjustYUY2BlitCoords)
            {
                srcPositions[4] *= 2;
            }

            pCommandList->SetGraphicsRoot32BitConstants(1, _countof(srcPositions), &srcPositions[0], 0);
        }

        // Constant buffers: srcPixelScalingFactor (For P010/Y410 UNORM pixel scaling to 10 bit range in [0.0f, 1.0f])
        {
            pCommandList->SetGraphicsRoot32BitConstants(2, 1, &srcPixelScalingFactor, 0);
        }

        pCommandList->SetPipelineState(pPSO->GetForUse(COMMAND_LIST_TYPE::GRAPHICS));

        CD3DX12_VIEWPORT Viewport((FLOAT)dstRect.left, (FLOAT)dstRect.top, (FLOAT)RectWidth(dstRect), (FLOAT)RectHeight(dstRect));
        CD3DX12_RECT Scissor(dstRect);
        pCommandList->RSSetViewports(1, &Viewport);
        pCommandList->RSSetScissorRects(1, &Scissor);

        pCommandList->DrawInstanced(4, 1, 0, 0);

        if (needsTwoPassColorConvert)
        {
            UINT srcSubresourceIndex = 0;
            Blit(pNewDestinationResource,
                &srcSubresourceIndex, 1,
                dstRect,
                pDst,
                pDstSubresourceIndices, numDstSubresources,
                dstRect,
                bEnableAlpha, bSwapRBChannels);
        }
        else if (needsTempRenderTarget)
        {
            D3D12_BOX srcBox = { 0, 0, 0, RectWidth(dstRect), RectHeight(dstRect), 1 };
            m_pParent->ResourceCopyRegion(
                pDst,
                pDstSubresourceIndices[0],
                dstRect.left,
                dstRect.top,
                0,
                pNewDestinationResource,
                0,
                &srcBox);
        }

        m_pParent->PostRender(COMMAND_LIST_TYPE::GRAPHICS, e_GraphicsStateDirty);
    }

    void BlitHelper::ResolveToNonMsaa( _Inout_ Resource **ppResource, _Inout_ UINT* pSubresourceIndices, UINT numSubresources )
    {
        auto pResource = *ppResource;
        assert( numSubresources == 1 ); // assert that it's only 1 because you can't have MSAA YUV resources.

        auto srcDesc = pResource->GetUnderlyingResource()->GetDesc();
        UINT width = static_cast<UINT>(srcDesc.Width);
        UINT height = static_cast<UINT>(srcDesc.Height);
        auto& cacheEntry = m_pParent->GetResourceCache().GetResource( pResource->AppDesc()->Format(), width, height );
        auto pCacheResource = cacheEntry.m_Resource.get();
        for (UINT i = 0; i < numSubresources; i++)
        {
            auto cacheSubresourceIndex = pCacheResource->GetSubresourceIndex( i, 0, 0 );
            m_pParent->ResourceResolveSubresource( pCacheResource, cacheSubresourceIndex, pResource, pSubresourceIndices[i], pResource->AppDesc()->Format() );
            pSubresourceIndices[i] = cacheSubresourceIndex;
        }
        *ppResource = pCacheResource;
    }
}