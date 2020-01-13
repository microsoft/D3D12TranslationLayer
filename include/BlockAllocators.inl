// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace BlockAllocators
{

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _OuterAllocator, class _SizeType>
CPooledBlockAllocator<_BlockType, _OuterAllocator, _SizeType>::CPooledBlockAllocator(_OuterAllocator &outerAllocator, _SizeType blockSize, SIZE_T maxItems)
    : m_freeStackSize(maxItems)
    , m_numFreeBlocks(0)
    , m_outerAllocator(outerAllocator)
    , m_blockSize(blockSize)
{
    m_pFreeBlockStack = new _BlockType[maxItems]; // throw(std::bad_alloc)
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _OuterAllocator, class _SizeType>
CPooledBlockAllocator<_BlockType, _OuterAllocator, _SizeType>::~CPooledBlockAllocator()
{
    Reset();
    delete[] m_pFreeBlockStack;
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _OuterAllocator, class _SizeType>
_BlockType CPooledBlockAllocator<_BlockType, _OuterAllocator, _SizeType>::Allocate(_SizeType size)
{
    if (size > m_blockSize)
    {
        // Can't allocate the requested size so return an empty block
        return _BlockType();
    }

    if (m_numFreeBlocks > 0)
    {
        m_numFreeBlocks--;
        return m_pFreeBlockStack[m_numFreeBlocks];
    }
    else
    {
        return m_outerAllocator.Allocate(m_blockSize);
    }
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _OuterAllocator, class _SizeType>
void CPooledBlockAllocator<_BlockType, _OuterAllocator, _SizeType>::Deallocate(const _BlockType &block)
{
    if (m_numFreeBlocks < m_freeStackSize)
    {
        m_pFreeBlockStack[m_numFreeBlocks] = block;
        m_numFreeBlocks++;
    }
    else
    {
        m_outerAllocator.Deallocate(block);
    }
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _OuterAllocator, class _SizeType>
void CPooledBlockAllocator<_BlockType, _OuterAllocator, _SizeType>::Reset()
{
    // Deallocate all the pooled objects
    for (unsigned i = 0; i < m_numFreeBlocks; ++i)
    {
        m_outerAllocator.Deallocate(m_pFreeBlockStack[i]);
    }
    m_numFreeBlocks = 0;
}

//================================================================================================
// class CBuddyAllocator
//================================================================================================

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _SizeType, _SizeType _MinBlockSize>
_SizeType CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize>::AllocateBlock(UINT order) // throw(std::bad_alloc)
{
    _SizeType offset;

    if (order > m_maxOrder)
    {
        throw(std::bad_alloc()); // Can't allocate a block that large
    }

    auto it = m_freeBlocks[order].begin();
    if (it == m_freeBlocks[order].end())
    {
        // No free nodes in the requested pool.  Try to find a higher-order block and split it.
        _SizeType left = AllocateBlock(order + 1); // throw(std::bad_alloc)
        _SizeType size = OrderToUnitSize(order);
        _SizeType right = left + size;
        m_freeBlocks[order].insert(right); // Add the right block to the free pool
        offset = left; // Return the left block
    }
    else
    {
        offset = *it;

        // Remove the block from the free list
        m_freeBlocks[order].erase(it);
    }

    return offset;
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _SizeType, _SizeType _MinBlockSize>
void CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize>::DeallocateBlock(_SizeType offset, UINT order) // throw(std::bad_alloc)
{
    // See if the buddy block is free
    _SizeType size = OrderToUnitSize(order);
    _SizeType buddy = GetBuddyOffset(offset, size);
    auto it = m_freeBlocks[order].find(buddy);
    if (it != m_freeBlocks[order].end())
    {
        // Deallocate merged blocks
        DeallocateBlock(min(offset, buddy), order + 1); // throw(std::bad_alloc)

        // Remove the buddy from the free list
        m_freeBlocks[order].erase(it);
    }
    else
    {
        // Add the block to the free list
        m_freeBlocks[order].insert(offset); // throw(std::bad_alloc)
    }
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _SizeType, _SizeType _MinBlockSize>
CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize>::CBuddyAllocator(_SizeType maxBlockSize, _SizeType baseOffset) // throw(std::bad_alloc)
    : m_baseOffset(baseOffset)
    , m_maxBlockSize(maxBlockSize)
    , m_pAllocatorData(new typename decltype(m_pAllocatorData)::element_type) // throw(std::bad_alloc)
{
    // maxBlockSize should be evenly dividable by _MinBlockSize and
    // maxBlockSize / _MinBlockSize should be a power of two
    assert((maxBlockSize / _MinBlockSize) * _MinBlockSize == maxBlockSize); // Evenly dividable
    assert(0 == ((maxBlockSize / _MinBlockSize) & ((maxBlockSize / _MinBlockSize) - 1))); // Power of two

    m_maxOrder = UnitSizeToOrder(SizeToUnitSize(maxBlockSize));

    Reset();
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _SizeType, _SizeType _MinBlockSize>
_BlockType CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize>::Allocate(_SizeType size)
{
    _SizeType unitSize = SizeToUnitSize(size);
    UINT order = UnitSizeToOrder(unitSize);
    try
    {
        _SizeType offset = AllocateBlock(order); // throw(std::bad_alloc)
        return _BlockType(m_baseOffset + (offset * _MinBlockSize), OrderToUnitSize(order) * _MinBlockSize);
    }
    catch (std::bad_alloc&)
    {
        // There are no blocks available for the requested size so
        // return the NULL block type
        return _BlockType(0, 0);
    }
}

//------------------------------------------------------------------------------------------------
template<class _BlockType, class _SizeType, _SizeType _MinBlockSize>
void CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize>::Deallocate(_In_ const _BlockType &block)
{
    assert(IsOwner(block));

    _SizeType offset = SizeToUnitSize(block.GetOffset() - m_baseOffset);
    _SizeType size = SizeToUnitSize(block.GetSize());
    UINT order = UnitSizeToOrder(size);

    try
    {
        DeallocateBlock(offset, order); // throw(std::bad_alloc)
    }
    catch (std::bad_alloc&)
    {
        // Deallocate failed trying to add the free block to the pool
        // resulting in a leak.  Unfortunately there is not much we can do.
        // Fortunately this is expected to be extremely rare as the storage
        // needed for each deallocate is very small.
    }
}

//================================================================================================
// class CBucketizedBlockAllocator
//================================================================================================

//------------------------------------------------------------------------------------------------
template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType>
CBucketizedBlockAllocator<_OuterAllocator, _SizeToBucketFunc, _BlockType, _SizeType>::CBucketizedBlockAllocator(SIZE_T numAllocators, _OuterAllocator *pAllocators)
    : m_pAllocators(pAllocators)
    , m_numAllocators(numAllocators)
{
}

template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType>
_BlockType CBucketizedBlockAllocator<_OuterAllocator, _SizeToBucketFunc, _BlockType, _SizeType>::Allocate(_SizeType size)
{
    _SizeToBucketFunc SizeToBucketFunc;
    SIZE_T bucket = SizeToBucketFunc(size);
    return bucket < m_numAllocators ? m_pAllocators[bucket].Allocate(size) : _BlockType();
}

template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType>
void CBucketizedBlockAllocator<_OuterAllocator, _SizeToBucketFunc, _BlockType, _SizeType>::Deallocate(const _BlockType &block)
{
    _SizeToBucketFunc SizeToBucketFunc;
    SIZE_T bucket = SizeToBucketFunc(block.GetSize());
    if (bucket < m_numAllocators)
    {
        m_pAllocators[bucket].Deallocate(block);
    }
}

template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType>
bool CBucketizedBlockAllocator<_OuterAllocator, _SizeToBucketFunc, _BlockType, _SizeType>::IsOwner(const _BlockType &block) const
{
    _SizeToBucketFunc SizeToBucketFunc;
    SIZE_T bucket = SizeToBucketFunc(block.GetSize());
    return bucket < this->m_numBuckets ? m_pAllocators[bucket].IsOwner(block) : false;
}

template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType>
void CBucketizedBlockAllocator<_OuterAllocator, _SizeToBucketFunc, _BlockType, _SizeType>::Reset()
{
    // Reset each of the outer allocators
    for (SIZE_T bucket = 0; bucket < m_numAllocators; ++bucket)
    {
        m_pAllocators[bucket].Reset();
    }
}

//================================================================================================
// class CDisjointBuddyAllocator
//================================================================================================
template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize> template <typename... InnerAllocatorArgs>
CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::CDisjointBuddyAllocator(_SizeType maxBlockSize, _SizeType threshold, InnerAllocatorArgs&&... innerArgs) // throw(std::bad_alloc)
    : m_BuddyAllocator(maxBlockSize)
    , m_Threshold(threshold)
    , m_InnerAllocator(std::forward<InnerAllocatorArgs>(innerArgs)...)
{
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
_BlockType CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::Allocate(_SizeType size) // throw(std::bad_alloc)
{
    if (size > m_Threshold)
    {
        throw(std::bad_alloc()); // Can't allocate a block that large
    }

    _BlockType block = m_BuddyAllocator.Allocate(size); // throw(std::bad_alloc)
    if (block.GetSize() == 0)
    {
        return block;
    }

    // This is just a way of making sure the deleter gets called unless release() gets called.
    std::unique_ptr<_BlockType, std::function<void(_BlockType*)>> blockGuard(&block,
        [this](_BlockType* b) { m_BuddyAllocator.Deallocate(*b); } );

    _SizeType offset = block.GetOffset();
    UINT bucket = BucketFromOffset(offset);

    if (bucket >= m_Allocations.size())
    {
        m_Allocations.resize(bucket + 1); // throw(std::bad_alloc)
    }

    if (m_Allocations[bucket].m_Refcount == 0)
    {
        m_Allocations[bucket].m_Allocation = m_InnerAllocator.Allocate(m_Threshold); // throw(std::bad_alloc)
    }

    // No more exceptions
    m_Allocations[bucket].m_Refcount++;
    blockGuard.release();

    return block;
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
void CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::Deallocate(const _BlockType &block)
{
    assert(IsOwner(block));
    
    _SizeType offset = block.GetOffset();
    UINT bucket = BucketFromOffset(offset);

    assert(bucket < m_Allocations.size());
    if (--m_Allocations[bucket].m_Refcount == 0)
    {
        m_InnerAllocator.Deallocate(m_Allocations[bucket].m_Allocation);
    }

    m_BuddyAllocator.Deallocate(block);
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
bool CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::IsOwner(const _BlockType &block) const
{
    return m_BuddyAllocator.IsOwner(block);
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
void CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::Reset()
{
    for (RefcountedAllocation& Allocation : m_Allocations)
    {
        if (Allocation.m_Refcount > 0)
        {
            m_InnerAllocator.Deallocate(Allocation.m_Allocation);
        }
    }
    m_Allocations.clear();
    m_BuddyAllocator.Reset();
    m_InnerAllocator.Reset();
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
auto CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::GetInnerAllocation(const _BlockType &block) const -> AllocationType
{
    assert(IsOwner(block));
    
    _SizeType offset = block.GetOffset();
    UINT bucket = BucketFromOffset(offset);

    assert(bucket < m_Allocations.size());
    assert(m_Allocations[bucket].m_Refcount > 0);
    return m_Allocations[bucket].m_Allocation;
}

template<class _BlockType, class _InnerAllocator, class _SizeType, _SizeType _MinBlockSize>
_SizeType CDisjointBuddyAllocator<_BlockType, _InnerAllocator, _SizeType, _MinBlockSize>::GetInnerAllocationOffset(const _BlockType &block) const
{
    assert(IsOwner(block));
    
    _SizeType offset = block.GetOffset();
    return offset % m_Threshold;
}

} // namespace BlockAllocators
