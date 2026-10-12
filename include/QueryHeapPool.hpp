// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class QueryHeapPool
    {
        struct Page;
    public:
        static constexpr UINT SlotsPerHeap = 4096;
        struct Allocation
        {
            ID3D12QueryHeap* Heap = nullptr;
            UINT BaseIndex = 0;
            Page* Owner = nullptr;
        };

        Allocation Allocate(ID3D12Device* device, const D3D12_QUERY_HEAP_DESC& desc, UINT64 completedFence) noexcept(false);
        void Release(Allocation& allocation, UINT64 lastUsedFence) noexcept;
        void Trim(UINT64 completedFence) noexcept;

    private:
        static constexpr UINT InvalidBlock = UINT_MAX;
        struct Block
        {
            UINT Next = InvalidBlock;
            UINT64 Fence = 0;
        };
        struct Page
        {
            unique_comptr<ID3D12QueryHeap> Heap;
            const D3D12_QUERY_HEAP_DESC Desc;
            const UINT SlotsPerAllocation;
            std::vector<Block> Blocks;
            UINT FreeHead = 0;
            UINT PendingHead = InvalidBlock;
            UINT FreeCount;
            explicit Page(const D3D12_QUERY_HEAP_DESC& desc);
            bool Matches(const D3D12_QUERY_HEAP_DESC& desc) const noexcept;
        };
        void Collect(UINT64 completedFence) noexcept;
        static void Free(Page& page, UINT block) noexcept;

        std::mutex m_Mutex;
        std::vector<std::unique_ptr<Page>> m_Pages;
        Page* m_AllocationPage = nullptr;
        UINT64 m_CompletedFence = 0;
    };
}
