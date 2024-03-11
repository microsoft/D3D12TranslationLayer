// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{

    class ImmediateContext;
    class Resource;

    enum ColorKeyType
    {
        COLORKEY_NONE,
        COLORKEY_SRC,
        COLORKEY_DEST
    };

    struct BlitColorKey
    {
        float ColorKey[4] = { 0.f,0.f,0.f,0.f };
        ColorKeyType Type = COLORKEY_NONE;
    };

    union BlitHelperKeyUnion
    {
        struct Bits
        {
            UINT SrcFormat : 8;
            UINT DstFormat : 8;
            UINT DstSampleCount : 4;
            UINT bEnableAlpha : 1;
            UINT bSwapRB : 1;
            UINT Unused : 10;
        } m_Bits;
        UINT m_Data;
    };
    static_assert(sizeof(BlitHelperKeyUnion) == sizeof(BlitHelperKeyUnion::m_Data));

    class BlitHelper
    {
    public:
        BlitHelper(ImmediateContext *pContext);
        void Blit(Resource* pSrc, UINT* pSrcSubresourceIndices, UINT numSrcSubresources, const RECT& srcRect, Resource* pDst, UINT* pDstSubresourceIndices, UINT numDstSubresources, const RECT& dstRect, bool bEnableAlpha = false, bool bSwapRBChannels = false, const BlitColorKey& colorKey = {});
        void Blit(Resource* pSrc, UINT SrcSubresourceIdx, const RECT& srcRect, Resource* pDst, UINT DstSubresourceIdx, const RECT& dstRect, bool bEnableAlpha = false, bool bSwapRBChannels = false);

    protected:
        using BlitPipelineState = DeviceChildImpl<ID3D12PipelineState>;
        void SelectPixelShader(const UINT& srcPlanes, CD3DX12_PIPELINE_STATE_STREAM_PS& outPS, const D3D12_RESOURCE_DESC& srcDesc, bool bSwapRB, ColorKeyType bltColorKeyType);
        BlitPipelineState* PrepareShaders(Resource *pSrc, UINT srcPlanes, Resource *pDst, UINT dstPlanes, bool bEnableAlpha, bool bSwapRB, const BlitColorKey& colorKey, int &outSrcPixelScalingFactor);

        ImmediateContext* const m_pParent;
        std::unordered_map<UINT, std::unique_ptr<BlitPipelineState>> m_spBlitPSOs;
        std::unique_ptr<InternalRootSignature> m_spRootSig;

    private:
        //@param ppResource: will be updated to point at the resolved resource
        //@param pSubresourceIndices: will be updated to reflect the resolved resource's subresource indecies
        void ResolveToNonMsaa( _Inout_ Resource **ppResource, _Inout_ UINT* pSubresourceIndices, UINT numSubresources );
    };
};