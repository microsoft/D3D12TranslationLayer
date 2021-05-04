#pragma once
#include "d3dkmthk.h"

namespace D3D12TranslationLayer
{
    class SwapChainHelper
    {
    public:
        SwapChainHelper( IDXGISwapChain3* swapChain );

        HRESULT StandardPresent( ImmediateContext& context, D3DKMT_PRESENT *pKMTPresent, Resource& presentingResource );

    private:
        IDXGISwapChain3* m_swapChain;   //weak-ref
    };
}