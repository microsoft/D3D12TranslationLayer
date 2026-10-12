// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    //----------------------------------------------------------------------------------------------------------------------------------
    QueryHeapPool::Page::Page(const D3D12_QUERY_HEAP_DESC& desc)
        : Desc{ desc.Type, (desc.Count > SlotsPerHeap ? 1 : SlotsPerHeap / desc.Count) * desc.Count, desc.NodeMask }
        , SlotsPerAllocation(desc.Count)
        , Blocks(Desc.Count / desc.Count)
        , FreeCount(static_cast<UINT>(Blocks.size()))
    {
        for (UINT i = 0; i + 1 < FreeCount; ++i)
            Blocks[i].Next = i + 1;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool QueryHeapPool::Page::Matches(const D3D12_QUERY_HEAP_DESC& desc) const noexcept
    {
        return Desc.Type == desc.Type && Desc.NodeMask == desc.NodeMask && SlotsPerAllocation == desc.Count;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void QueryHeapPool::Free(Page& page, UINT block) noexcept
    {
        page.Blocks[block].Next = page.FreeHead;
        page.FreeHead = block;
        ++page.FreeCount;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void QueryHeapPool::Collect(UINT64 completedFence) noexcept
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
    QueryHeapPool::Allocation QueryHeapPool::Allocate(
        ID3D12Device* device, const D3D12_QUERY_HEAP_DESC& desc, UINT64 completedFence) noexcept(false)
    {
        ThrowFailure(device && desc.Count ? S_OK : E_INVALIDARG); // throw( _com_error )
        std::lock_guard<std::mutex> lock(m_Mutex);
        Collect(completedFence);
        if (!m_AllocationPage || !m_AllocationPage->FreeCount || !m_AllocationPage->Matches(desc))
        {
            m_AllocationPage = nullptr;
            for (auto& page : m_Pages)
            {
                if (page->FreeCount && page->Matches(desc))
                {
                    m_AllocationPage = page.get();
                    break;
                }
            }
            if (!m_AllocationPage)
            {
                auto page = std::make_unique<Page>(desc); // throw( std::bad_alloc )
                ThrowFailure(device->CreateQueryHeap(&page->Desc, IID_PPV_ARGS(&page->Heap))); // throw( _com_error )
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
        return { page.Heap.get(), block * page.SlotsPerAllocation, &page };
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void QueryHeapPool::Release(Allocation& allocation, UINT64 lastUsedFence) noexcept
    {
        if (!allocation.Owner)
            return;
        std::lock_guard<std::mutex> lock(m_Mutex);
        Page& page = *allocation.Owner;
        const UINT block = allocation.BaseIndex / page.SlotsPerAllocation;
        // Releasing a query on the CPU does not mean the GPU has finished using
        // its slots. Reuse them only after the owning queue completes its last submission.
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
    void QueryHeapPool::Trim(UINT64 completedFence) noexcept
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        Collect(completedFence);
        // Retain one empty heap per type, allocation size and node mask for reuse.
        for (auto it = m_Pages.begin(); it != m_Pages.end();)
        {
            bool redundant = false;
            if ((*it)->FreeCount == (*it)->Blocks.size())
            {
                const D3D12_QUERY_HEAP_DESC key = { (*it)->Desc.Type, (*it)->SlotsPerAllocation, (*it)->Desc.NodeMask };
                for (auto prior = m_Pages.begin(); prior != it; ++prior)
                {
                    if ((*prior)->FreeCount == (*prior)->Blocks.size() && (*prior)->Matches(key))
                    {
                        redundant = true;
                        break;
                    }
                }
            }
            if (redundant)
            {
                if (m_AllocationPage == it->get())
                    m_AllocationPage = nullptr;
                it = m_Pages.erase(it);
            }
            else
                ++it;
        }
    }
}
