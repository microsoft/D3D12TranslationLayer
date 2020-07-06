// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer 
{
    class HeapSuballocationBlock : public BlockAllocators::CGenericBlock<UINT64>
    {
    public:
        HeapSuballocationBlock() : BlockAllocators::CGenericBlock<UINT64>(), m_pDirectHeapAllocation(nullptr) {}
        HeapSuballocationBlock(UINT64 newOffset, UINT64 newSize, ID3D12Resource *pResource = nullptr) : BlockAllocators::CGenericBlock<UINT64>(newOffset, newSize), m_pDirectHeapAllocation(pResource) {}

        bool IsDirectAllocation() const { return m_pDirectHeapAllocation; }
        ID3D12Resource *GetDirectHeapAllocation() const { assert(IsDirectAllocation()); return m_pDirectHeapAllocation; }
    private:
        ID3D12Resource *m_pDirectHeapAllocation;
    };

    class ImmediateContext; // Forward Declaration
    class InternalHeapAllocator
    {
    public:
        InternalHeapAllocator(ImmediateContext *pContext, AllocatorHeapType heapType) :
            m_pContext(pContext), m_HeapType(heapType) {}

        ID3D12Resource* Allocate(UINT64 size);
        void Deallocate(ID3D12Resource* pResource);
    private:
        ImmediateContext *m_pContext;
        AllocatorHeapType m_HeapType;
    };

    // Doesn't do any suballocation and instead just allocates a whole new 
    // resource directly for each call to Allocate()
    // Note: Because it doesn't suballocate at all, it is thread safe by default
    template<class _BlockType, class _InnerAllocator, class _SizeType>
    class DirectAllocator
    {
    private:
        using InnerAllocatorDecayed = typename std::decay<_InnerAllocator>::type;
    public:
        template <typename... InnerAllocatorArgs>
        DirectAllocator(InnerAllocatorArgs&&... innerArgs) : // throw(std::bad_alloc)
            m_InnerAllocator(std::forward<InnerAllocatorArgs>(innerArgs)...) {}
        DirectAllocator() = default;
        DirectAllocator(DirectAllocator&&) = default;
        DirectAllocator& operator=(DirectAllocator&&) = default;

        _BlockType Allocate(_SizeType size) { 
            _BlockType block(0, size, m_InnerAllocator.Allocate(size));
            return block;
        }
        void Deallocate(const _BlockType &block) { m_InnerAllocator.Deallocate(block.GetDirectHeapAllocation()); }

        typedef typename std::invoke_result<decltype(&InnerAllocatorDecayed::Allocate), InnerAllocatorDecayed, _SizeType>::type AllocationType;

        inline bool IsOwner(_In_ const _BlockType &block) const { return block.GetOffset() == 0; }
        AllocationType GetInnerAllocation(const _BlockType &block) const { return block.GetDirectHeapAllocation(); }
        _SizeType GetInnerAllocationOffset(const _BlockType &block) const { return 0; }
    private:
        _InnerAllocator m_InnerAllocator;
    };

    typedef DirectAllocator<HeapSuballocationBlock, InternalHeapAllocator, UINT64> DirectHeapAllocator;
    typedef BlockAllocators::CDisjointBuddyAllocator<HeapSuballocationBlock, InternalHeapAllocator, UINT64, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT> DisjointBuddyHeapAllocator;

    class ThreadSafeBuddyHeapAllocator : DisjointBuddyHeapAllocator
    {
    public:
        template <typename... InnerAllocatorArgs>
        ThreadSafeBuddyHeapAllocator(UINT64 maxBlockSize, UINT64 threshold, bool bNeedsThreadSafety, InnerAllocatorArgs&&... innerArgs) : // throw(std::bad_alloc)
            DisjointBuddyHeapAllocator(maxBlockSize, threshold, std::forward<InnerAllocatorArgs>(innerArgs)...),
            m_Lock(bNeedsThreadSafety)
        {}
        ThreadSafeBuddyHeapAllocator() = default;
        ThreadSafeBuddyHeapAllocator(ThreadSafeBuddyHeapAllocator&&) = default;
        ThreadSafeBuddyHeapAllocator& operator=(ThreadSafeBuddyHeapAllocator&&) = default;
        
        HeapSuballocationBlock Allocate(UINT64 size)
        {
            auto scopedLock = m_Lock.TakeLock();
            return DisjointBuddyHeapAllocator::Allocate(size);
        }

        void Deallocate(const HeapSuballocationBlock &block)
        {
            auto scopedLock = m_Lock.TakeLock();
            DisjointBuddyHeapAllocator::Deallocate(block);
        }

        auto GetInnerAllocation(const HeapSuballocationBlock &block) const
        {
            auto scopedLock = m_Lock.TakeLock();
            return DisjointBuddyHeapAllocator::GetInnerAllocation(block);
        }

        // Exposing methods that don't require locks.
        using DisjointBuddyHeapAllocator::IsOwner;

    private:
        OptLock<> m_Lock;
    };

    // Allocator that will conditionally choose to individually allocate resources or suballocate based on a 
    // passed in function
    template <class _BlockType, class _SizeType, class DirectAllocator, class SuballocationAllocator, class AllocationArgs>
    class ConditionalAllocator
    {
    public:
        typedef bool(*RequiresDirectAllocationFunctionType)(typename _SizeType, typename AllocationArgs);

        template <typename... SuballocationAllocatorArgs, typename... DirectAllocatorArgs>
        ConditionalAllocator(std::tuple<SuballocationAllocatorArgs...> suballocatedAllocatorArgs,
            std::tuple<DirectAllocatorArgs...> directHeapAllocatorArgs,
            RequiresDirectAllocationFunctionType pfnRequiresDirectHeapAllocator)
            : m_DirectAllocator(std::get<DirectAllocatorArgs>(directHeapAllocatorArgs)...),
            m_SuballocationAllocator(std::get<SuballocationAllocatorArgs>(suballocatedAllocatorArgs)...),
            m_pfnUseDirectHeapAllocator(pfnRequiresDirectHeapAllocator)
        {
        }

        _BlockType Allocate(_SizeType size, AllocationArgs args)
        {
            if (m_pfnUseDirectHeapAllocator(size, args)) { return m_DirectAllocator.Allocate(size); }
            else { return m_SuballocationAllocator.Allocate(size); }
        }

        void Deallocate(const _BlockType &block)
        {
            assert(IsOwner(block));
            if (block.IsDirectAllocation()) { m_DirectAllocator.Deallocate(block); }
            else { m_SuballocationAllocator.Deallocate(block); }
        }

        bool IsOwner(const _BlockType &block) const
        {
            if (block.IsDirectAllocation()) { return m_DirectAllocator.IsOwner(block); }
            else { return m_SuballocationAllocator.IsOwner(block); }
        }

        void Reset()
        {
            m_DirectAllocator.Reset();
            m_SuballocationAllocator.Reset();
        }

        auto GetInnerAllocation(const _BlockType &block) const
        {
            assert(IsOwner(block));
            if (block.IsDirectAllocation()) { return m_DirectAllocator.GetInnerAllocation(block); }
            else { return m_SuballocationAllocator.GetInnerAllocation(block); }
        }

        _SizeType GetInnerAllocationOffset(const _BlockType &block) const
        {
            assert(IsOwner(block));
            if (block.IsDirectAllocation()) { return m_DirectAllocator.GetInnerAllocationOffset(block); }
            else { return m_SuballocationAllocator.GetInnerAllocationOffset(block); }
        }

    private:
        SuballocationAllocator m_SuballocationAllocator;
        DirectAllocator m_DirectAllocator;
        RequiresDirectAllocationFunctionType m_pfnUseDirectHeapAllocator;
    };
    static constexpr UINT cBuddyAllocatorThreshold = 64 * 1024;
}
