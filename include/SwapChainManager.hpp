#pragma once

namespace D3D12TranslationLayer
{
    class SwapChainManager
    {
    public:
        SwapChainManager( D3D12TranslationLayer::ImmediateContext& ImmCtx );
        ~SwapChainManager();
        IDXGISwapChain3* GetSwapChainForWindow( HWND hwnd, Resource& presentingResource );

    private:
        static constexpr UINT BufferCount = 2;
        D3D12TranslationLayer::ImmediateContext& m_ImmCtx;
        std::map<HWND, unique_comptr<IDXGISwapChain3>> m_SwapChains;
    };
}