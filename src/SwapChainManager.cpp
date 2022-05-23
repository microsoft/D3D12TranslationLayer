#include "pch.h"
#include "SwapChainManager.hpp"

namespace D3D12TranslationLayer
{
    SwapChainManager::SwapChainManager( D3D12TranslationLayer::ImmediateContext& ImmCtx ) : m_ImmCtx( ImmCtx ) {}

    SwapChainManager::~SwapChainManager()
    {
        bool success = false;
        try {
            success = m_ImmCtx.WaitForCompletion(D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK); // throws
        }
        catch (_com_error&)
        {
            success = false;
        }
        catch (std::bad_alloc&)
        {
            success = false;
        }
    }

    IDXGISwapChain3* SwapChainManager::GetSwapChainForWindow(HWND hwnd, Resource& presentingResource)
    {
        auto& spSwapChain = m_SwapChains[hwnd];
        auto pResourceDesc = presentingResource.AppDesc();
        DXGI_SWAP_CHAIN_DESC Desc = {};
        //SwapChain creation fails if using BGRX, so pretend that it's BGRA. Present still works as expected.
        DXGI_FORMAT format = pResourceDesc->Format() == DXGI_FORMAT_B8G8R8X8_UNORM ? DXGI_FORMAT_B8G8R8A8_UNORM : pResourceDesc->Format();
        if (spSwapChain)
        {
            spSwapChain->GetDesc( &Desc );
            if (Desc.BufferDesc.Format != format ||
                 Desc.BufferDesc.Width != pResourceDesc->Width() ||
                 Desc.BufferDesc.Height != pResourceDesc->Height())
            {
                m_ImmCtx.WaitForCompletion( D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK ); // throws
                ThrowFailure( spSwapChain->ResizeBuffers( BufferCount,
                                                          pResourceDesc->Width(),
                                                          pResourceDesc->Height(),
                                                          format,
                                                          DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING ) );
            }
        }
        else
        {
            Desc.BufferCount = BufferCount;
            
            Desc.BufferDesc.Format = format;
            Desc.BufferDesc.Width = pResourceDesc->Width();
            Desc.BufferDesc.Height = pResourceDesc->Height();
            Desc.OutputWindow = hwnd;
            Desc.Windowed = 1;
            Desc.SampleDesc.Count = 1;
            Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            unique_comptr<IDXGIFactory> spFactory;
            ThrowFailure( CreateDXGIFactory2( 0, IID_PPV_ARGS( &spFactory ) ) );
            unique_comptr<IDXGISwapChain> spBaseSwapChain;
            ThrowFailure( spFactory->CreateSwapChain(
                m_ImmCtx.GetCommandQueue( D3D12TranslationLayer::COMMAND_LIST_TYPE::GRAPHICS ),
                &Desc, &spBaseSwapChain ) );
            ThrowFailure( spBaseSwapChain->QueryInterface( &spSwapChain ) );
        }
        return spSwapChain.get();
    }

    void SwapChainManager::SetMaximumFrameLatency( UINT MaxFrameLatency )
    {
        m_MaximumFrameLatency = MaxFrameLatency;
    }

    bool SwapChainManager::IsMaximumFrameLatencyReached()
    {
        UINT64 CompletedFenceValue = m_ImmCtx.GetCompletedFenceValue( D3D12TranslationLayer::COMMAND_LIST_TYPE::GRAPHICS );
        while (m_PresentFenceValuesBegin != m_PresentFenceValuesEnd &&
                *m_PresentFenceValuesBegin <= CompletedFenceValue)
        {
            ++m_PresentFenceValuesBegin;
        }
        return std::distance( m_PresentFenceValuesBegin, m_PresentFenceValuesEnd ) >= (ptrdiff_t)m_MaximumFrameLatency;
    }

    void SwapChainManager::WaitForMaximumFrameLatency()
    {
        // Looping, because max frame latency can be dropped, and we may
        // need to wait for multiple presents to complete here.
        while (IsMaximumFrameLatencyReached())
        {
            m_ImmCtx.WaitForFenceValue(
                D3D12TranslationLayer::COMMAND_LIST_TYPE::GRAPHICS,
                *m_PresentFenceValuesBegin );
        }
    }
}