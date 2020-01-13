// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    ID3D12Resource* InternalHeapAllocator::Allocate(UINT64 size)
    {
        // Transfer ownership of ID3D12Resource to the calling allocator
        return m_pContext->AcquireTransitionableUploadBuffer(m_HeapType, size).release();
    }

    void InternalHeapAllocator::Deallocate(ID3D12Resource* pResource)
    {
        m_pContext->ReturnTransitionableBufferToPool(
            m_HeapType, 
            pResource->GetDesc().Width, 
            std::move(unique_comptr<ID3D12Resource>(pResource)), 
            // Guaranteed to be finished since this is only called after
            // all suballocations have been through the deferred deletion queue
            m_pContext->GetCompletedFenceValue(CommandListType(m_HeapType)));

        // Leave ownership to the buffer pool
        pResource->Release();
    }

}
