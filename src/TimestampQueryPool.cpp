// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    //----------------------------------------------------------------------------------------------------------------------------------
    TimestampQueryPool::Page::Page()
    {
        for (UINT i = 0; i + 1 < BlocksPerHeap; ++i)
            Blocks[i].Next = i + 1;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampQueryPool::Free(Page& page, UINT block) noexcept
    {
        page.Blocks[block].Next = page.FreeHead;
        page.FreeHead = block;
        ++page.FreeCount;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampQueryPool::Collect(UINT64 completedFence) noexcept
    {
        // Avoid repeatedly walking pending blocks while the GPU has not advanced.
        if (completedFence <= m_CompletedFence)
            return;
        m_CompletedFence = completedFence;
        for (auto& page : m_Pages)
        {
            UINT* link = &page->PendingHead;
            while (*link != InvalidBlock)
            {
                const UINT block = *link;
                if (page->Blocks[block].Fence <= completedFence)
                {
                    *link = page->Blocks[block].Next;
                    Free(*page, block);
                }
                else
                    link = &page->Blocks[block].Next;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    TimestampQueryPool::Allocation TimestampQueryPool::Allocate(
        ID3D12Device* device, UINT nodeMask, UINT64 completedFence) noexcept(false)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        Collect(completedFence);
        if (!m_AllocationPage || !m_AllocationPage->FreeCount)
        {
            m_AllocationPage = nullptr;
            for (auto& page : m_Pages)
            {
                if (page->FreeCount)
                {
                    m_AllocationPage = page.get();
                    break;
                }
            }
            if (!m_AllocationPage)
            {
                auto page = std::make_unique<Page>(); // throw( std::bad_alloc )
                D3D12_QUERY_HEAP_DESC desc = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, SlotsPerHeap, nodeMask };
                ThrowFailure(device->CreateQueryHeap(&desc, IID_PPV_ARGS(&page->Heap))); // throw( _com_error )
                // Commit ownership before handing out a block; allocation failures
                // leave the existing pool unchanged and release the new heap.
                m_Pages.push_back(std::move(page)); // throw( std::bad_alloc )
                m_AllocationPage = m_Pages.back().get();
            }
        }
        Page& page = *m_AllocationPage;
        const UINT block = page.FreeHead;
        page.FreeHead = page.Blocks[block].Next;
        --page.FreeCount;
        return { page.Heap.get(), block * SlotsPerQuery, &page };
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampQueryPool::Release(Allocation& allocation, UINT64 lastUsedFence) noexcept
    {
        if (!allocation.Owner)
            return;
        std::lock_guard<std::mutex> lock(m_Mutex);
        Page& page = *allocation.Owner;
        const UINT block = allocation.BaseIndex / SlotsPerQuery;
        // Releasing a query on the CPU does not mean the GPU has finished using
        // its slots. Reuse them only after the last graphics submission completes.
        if (lastUsedFence <= m_CompletedFence)
            Free(page, block);
        else
        {
            // Intrusive pending/free lists require no allocation during destruction.
            page.Blocks[block].Fence = lastUsedFence;
            page.Blocks[block].Next = page.PendingHead;
            page.PendingHead = block;
        }
        allocation = {};
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void TimestampQueryPool::Trim(UINT64 completedFence) noexcept
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        Collect(completedFence);
        // Retain one fully free heap to avoid recreating it for subsequent queries;
        // release the remaining empty heaps.
        bool retainedEmptyPage = false;
        for (auto it = m_Pages.begin(); it != m_Pages.end();)
        {
            if ((*it)->FreeCount == BlocksPerHeap)
            {
                if (retainedEmptyPage)
                {
                    if (m_AllocationPage == it->get())
                        m_AllocationPage = nullptr;
                    it = m_Pages.erase(it);
                    continue;
                }
                retainedEmptyPage = true;
            }
            ++it;
        }
    }
}
