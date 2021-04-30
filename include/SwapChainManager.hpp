#pragma once

namespace D3D12TranslationLayer
{
    class SwapChainManager
    {
    public:
        SwapChainManager( D3D12TranslationLayer::ImmediateContext& ImmCtx );
        ~SwapChainManager();
        IDXGISwapChain3* GetSwapChainForWindow( HWND hwnd, Resource& presentingResource );
        
        void SetMaximumFrameLatency( UINT MaxFrameLatency );
        bool IsMaximumFrameLatencyReached();
        void WaitForMaximumFrameLatency();

    private:
        static constexpr UINT BufferCount = 2;
        D3D12TranslationLayer::ImmediateContext& m_ImmCtx;
        std::map<HWND, unique_comptr<IDXGISwapChain3>> m_SwapChains;
        UINT m_MaximumFrameLatency = 3;
        CircularArray<UINT64, 17> m_PresentFenceValues = {};
        // The fence value to wait on
        decltype(m_PresentFenceValues)::iterator m_PresentFenceValuesBegin = m_PresentFenceValues.begin();
        // The fence value to write to
        decltype(m_PresentFenceValues)::iterator m_PresentFenceValuesEnd = m_PresentFenceValuesBegin;
    };
}