#include "pch.h"
#include "SwapChainHelper.hpp"

namespace D3D12TranslationLayer
{
    SwapChainHelper::SwapChainHelper( IDXGISwapChain3* swapChain ) : m_swapChain( swapChain )
    {
    }

    HRESULT SwapChainHelper::StandardPresent( ImmediateContext& context, D3DKMT_PRESENT *pKMTPresent, Resource& presentingResource )
    {
        unique_comptr<ID3D12Resource> backBuffer;
        m_swapChain->GetBuffer( m_swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS( &backBuffer ) );

        D3D12TranslationLayer::ResourceCreationArgs destArgs = *presentingResource.Parent();
        destArgs.m_appDesc.m_Samples = 1;
        destArgs.m_appDesc.m_bindFlags = D3D12TranslationLayer::RESOURCE_BIND_RENDER_TARGET;
        destArgs.m_desc12.SampleDesc.Count = 1;
        destArgs.m_desc12.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto destResource = D3D12TranslationLayer::Resource::OpenResource(
            &context,
            destArgs,
            backBuffer.get(),
            D3D12TranslationLayer::DeferredDestructionType::Submission,
            D3D12_RESOURCE_STATE_COMMON );
        D3D12_RESOURCE_STATES OperationState;
        if (presentingResource.AppDesc()->Samples() > 1)
        {
            context.ResourceResolveSubresource( destResource.get(), 0, &presentingResource, 0, destArgs.m_appDesc.Format() );
            OperationState = D3D12_RESOURCE_STATE_RESOLVE_DEST;
        }
        else
        {
            context.ResourceCopy( destResource.get(), &presentingResource );
            OperationState = D3D12_RESOURCE_STATE_COPY_DEST;
        }
        D3D12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition( backBuffer.get(), OperationState, D3D12_RESOURCE_STATE_COMMON );
        context.GetGraphicsCommandList()->ResourceBarrier( 1, &Barrier );
        context.Flush( D3D12TranslationLayer::COMMAND_LIST_TYPE_ALL_MASK );

        HRESULT hr = m_swapChain->Present( pKMTPresent->FlipInterval, pKMTPresent->FlipInterval == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0 );
        context.GetCommandListManager( D3D12TranslationLayer::COMMAND_LIST_TYPE::GRAPHICS )->SetNeedSubmitFence();

        return hr;
    }
}