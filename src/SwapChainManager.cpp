#include "pch.h"
#include "SwapChainManager.hpp"

namespace D3D12TranslationLayer
{
    SwapChainManager::SwapChainManager( D3D12TranslationLayer::ImmediateContext& ImmCtx ) : m_ImmCtx( ImmCtx ) {}

    SwapChainManager::~SwapChainManager()
    {
        m_ImmCtx.WaitForCompletion( D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK );
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
                m_ImmCtx.WaitForCompletion( D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK );
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
}