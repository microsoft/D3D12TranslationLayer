// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include <memory>

//================================================================================================
// Allocator classes must implement:
//  _BlockType Allocate(_SizeType size)
//  void Deallocate(const _BlockType &block)
//  bool IsOwner(const _BlockType &block) const
//  void Reset()
//
// _SizeType must be initializable to 0
//
// _BlockType classes  must implement the following functions:
//  <default constructor> // represents an empty block
//  <copy constructor>
//  _SizeType GetSize() const // empty blocks should return 0
//  _SizeType GetOffset() const // empty blocks should return 0
//================================================================================================

namespace BlockAllocators
{

//================================================================================================
// Generic block class compatible with any allocator class.
// Can be used directly or as a base class for custom allocators.
template<class _SizeType = SIZE_T>
class CGenericBlock
{
    _SizeType m_offset = 0;
    _SizeType m_size = 0;

public:
    CGenericBlock() {}
    CGenericBlock(_SizeType offset, _SizeType size)
        : m_size(size)
        , m_offset(offset) {}

    CGenericBlock(const CGenericBlock &, _SizeType newOffset, _SizeType newSize)
        : m_size(newSize)
        , m_offset(newOffset) {}

    bool operator==(const CGenericBlock &o) const
    {
        return m_size == o.m_size && m_offset == o.m_offset;
    }

    bool IsSuballocatedFrom(const CGenericBlock &o) const
    {
        return m_offset >= o.m_offset
            && m_offset + m_size <= o.m_offset + o.m_size
            && m_offset + m_size >= m_offset // overflow check
            && o.m_offset + o.m_size >= o.m_offset; // overflow check
    }

    _SizeType GetSize() const { return m_size; }
    _SizeType GetOffset() const { return m_offset; }
};

//================================================================================================
// Allocates a block with 0 size and 0 offset
//
// Template parameters
// _BlockType - Block class type
// _SizeType - Offset and Size types used by the block
template<class _BlockType, class _SizeType = SIZE_T>
class CNullBlockAllocator
{
public:
    _BlockType Allocate(_SizeType size) { return _BlockType; /* Returns a default block*/ }
    void Deallocate(_BlockType &block) { assert(block.GetSize() == 0); }
    bool IsOwner(_BlockType &block) const { return block.GetSize() == 0 && block.GetOffset() == 0; }
    void Reset() {}
};

//================================================================================================
// Allocates uniform blocks out of a stack-based pool if available and returns blocks
// back to the pool on deallocate.  
//
// On allocate the outer allocator is used if the pool is empty.
//
// On deallocate the outer allocator is used if the pool is full.
//
// Template parameters
// _BlockType - Block class type
// _SizeType - Offset and Size types used by the block
// _OuterAllocator - Allocator to fall-back to when the pool is unavailable
// _MaxItems - Integer indicating the limit of number of stored blocks in the pool
template<class _BlockType, class _OuterAllocator, class _SizeType = SIZE_T>
class CPooledBlockAllocator
{
private:
    SIZE_T m_numFreeBlocks = 0;
    SIZE_T m_freeStackSize = 0;
    _BlockType *m_pFreeBlockStack = nullptr; // Holds pointers to the free blocks
    _OuterAllocator &m_outerAllocator;
    _SizeType m_blockSize = 0;

public:
    CPooledBlockAllocator(_OuterAllocator &outerAllocator, _SizeType blockSize, SIZE_T maxItems);
    ~CPooledBlockAllocator();

    // Required Allocator functions
    _BlockType Allocate(_SizeType size);
    void Deallocate(const _BlockType &block);
    bool IsOwner(const _BlockType &block) const { return block.GetSize() == blockSize; }
    void Reset();
};

//================================================================================================
inline UINT Log2Ceil(UINT64 value)
{
    UINT ceil;
    unsigned char bsrResult;

    if (value == 0)
    {
        return (UINT)-1;
    }

#if defined(_WIN64)
    bsrResult = _BitScanReverse64((ULONG *)&ceil, value);
    assert(ceil != (UINT)-1);
    assert(bsrResult != 0);
#else
    UINT upper = (UINT)(value >> 32);
    if (0 == upper)
    {
        UINT lower = (UINT)value;
        bsrResult = _BitScanReverse((ULONG *)&ceil, lower);
        assert(ceil != (UINT)-1);
        assert(bsrResult != 0);
    }
    else
    {
        bsrResult = _BitScanReverse((ULONG *)&ceil, upper);
        assert(ceil != (UINT)-1);
        assert(bsrResult != 0);

        ceil += 32;
    }
#endif
    // Add 1 unless value is a power of two
    return ceil + (value & (value - 1) ? 1 : 0);
}

//================================================================================================
inline UINT Log2Ceil(UINT32 value)
{
    UINT ceil;
    if (0 == _BitScanReverse((ULONG *)&ceil, value))
    {
        ceil = (UINT)-1;
    }
    // Add 1 unless value is a power of two
    return ceil + (value & (value - 1) ? 1 : 0);
}

#ifndef _WIN64
inline UINT32 Log2Ceil(SIZE_T value)
{
    return Log2Ceil(static_cast<UINT32>(value));
}
#endif

//================================================================================================
template <size_t PoolSizeInElements = 1024>
class PoolingStdAllocatorData
{
    std::vector<std::unique_ptr<BYTE[]>> m_pools;
    BYTE* m_pHead = nullptr;

    template <class T>
    void AllocPool() // throw(std::bad_alloc)
    {
        constexpr size_t PoolSizeInBytes = sizeof(T) * PoolSizeInElements;

        assert(m_pHead == nullptr);
        m_pools.push_back(std::unique_ptr<BYTE[]>(new BYTE[PoolSizeInBytes])); // throw(std::bad_alloc)
        m_pHead = m_pools.back().get();

        // Turn the block into a linked list
        auto pCurPointer = reinterpret_cast<void**>(m_pHead);
        auto pNextPointer = m_pHead + sizeof(T);
        for (size_t i = 0; i < PoolSizeInElements - 1; ++i)
        {
            *pCurPointer = pNextPointer;
            pCurPointer = reinterpret_cast<void**>(pNextPointer);
            pNextPointer += sizeof(T);
        }
        assert(pNextPointer == m_pHead + PoolSizeInBytes);
        *pCurPointer = nullptr;
    }

public:
    template <class T>
    T* allocate(size_t size) // throw(std::bad_alloc)
    {
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
        return reinterpret_cast<T*>(::operator new(sizeof(T) * size));
#else
        UNREFERENCED_PARAMETER(size);
        assert(size == 1);
        if (m_pHead == nullptr)
        {
            AllocPool<T>(); // throw(std::bad_alloc)
        }
        T* pRet = reinterpret_cast<T*>(m_pHead);
        m_pHead = *reinterpret_cast<BYTE**>(m_pHead);
        return pRet;
#endif
    }

    template <class T>
    void deallocate(T* p, size_t)
    {
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
        return ::operator delete(p);
#else
#if TRANSLATION_LAYER_DBG
        memset(p, 0xAF, sizeof(T));
#endif
        *reinterpret_cast<BYTE**>(p) = m_pHead;
        m_pHead = reinterpret_cast<BYTE*>(p);
#endif
    }
};

//================================================================================================
// An STL-compatible allocator for pooling data
template <class T, size_t PoolSizeInElements = 1024>
class PoolingStdAllocator
{
public:
    using UnderlyingAllocatorImpl = PoolingStdAllocatorData<PoolSizeInElements>;
    using value_type = T;

    template <typename U> struct rebind
    {
        using other = PoolingStdAllocator<U, PoolSizeInElements>;
    };

    UnderlyingAllocatorImpl* m_pAllocator;
    T* allocate(size_t size)
    {
        return m_pAllocator->allocate<T>(size);
    }
    void deallocate(T* p, size_t size)
    {
        m_pAllocator->deallocate<T>(p, size);
    }

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    bool operator==(PoolingStdAllocator const& o) const { return o.m_pAllocator == m_pAllocator; }
    bool operator!=(PoolingStdAllocator const& o) const { return o.m_pAllocator != m_pAllocator; }

    explicit PoolingStdAllocator(UnderlyingAllocatorImpl& underlying)
        : m_pAllocator(&underlying) { }

    template <class U>
    PoolingStdAllocator(PoolingStdAllocator<U, PoolSizeInElements> const& other)
        : m_pAllocator(other.m_pAllocator) { }

    template <class U>
    PoolingStdAllocator& operator=(PoolingStdAllocator<U, PoolSizeInElements> const& other)
    {
        m_pAllocator = other.m_pAllocator;
        return *this;
    }
};

//================================================================================================
// Allocates blocks from a fixed range using buddy allocation method.
// Buddy allocation allows reasonably fast allocation of arbitrary size blocks
// with minimal fragmentation and provides efficient reuse of freed ranges.
//
// Template parameters
// _BlockType - Block class type
// _SizeType - Offset and Size types used by the block
// _MinBlockSise - Smallest allocatable block size
template<class _BlockType, class _SizeType = SIZE_T, _SizeType _MinBlockSize = 1>
class CBuddyAllocator
{
    std::unique_ptr<PoolingStdAllocatorData<>> m_pAllocatorData;
    using SetType = std::set<_SizeType, std::less<_SizeType>, PoolingStdAllocator<_SizeType>>;
    std::vector<SetType> m_freeBlocks;
    UINT m_maxOrder = 0;
    _SizeType m_baseOffset = 0;
    _SizeType m_maxBlockSize = 0;

    inline _SizeType SizeToUnitSize(_SizeType size) const
    {
        return (size + (_MinBlockSize - 1)) / _MinBlockSize;
    }

    inline UINT UnitSizeToOrder(_SizeType size) const
    {
        return Log2Ceil(size);
    }

    inline _SizeType GetBuddyOffset(const _SizeType &offset, const _SizeType &size)
    {
        return offset ^ size;
    }

    inline _SizeType OrderToUnitSize(UINT order) const { return ((_SizeType)1) << order; }

    inline _SizeType AllocateBlock(UINT order); // throw(std::bad_alloc)
    inline void DeallocateBlock(_SizeType offset, UINT order); // throw(std::bad_alloc)

public:
    CBuddyAllocator(_SizeType maxBlockSize, _SizeType baseOffset = 0); // throw(std::bad_alloc)
    CBuddyAllocator() = default;
    // Noncopyable
    CBuddyAllocator(CBuddyAllocator const&) = delete;
    CBuddyAllocator(CBuddyAllocator&&) = default;
    CBuddyAllocator& operator=(CBuddyAllocator const&) = delete;
    CBuddyAllocator& operator=(CBuddyAllocator&&) = default;

    inline _BlockType Allocate(_SizeType size);
    inline void Deallocate(_In_ const _BlockType &block);

    inline bool IsOwner(_In_ const _BlockType &block) const
    {
        return block.GetOffset() >= m_baseOffset && block.GetSize() <= m_maxBlockSize;
    }

    inline void Reset()
    {
        // Clear the free blocks collection
        m_freeBlocks.clear();

        // Initialize the pool with a free inner block of max inner block size
        m_freeBlocks.reserve(m_maxOrder + 1); // throw(std::bad_alloc)
        for (UINT i = 0; i <= m_maxOrder; ++i)
        {
            m_freeBlocks.emplace_back(PoolingStdAllocator<_SizeType>(*m_pAllocatorData));
        }
        m_freeBlocks[m_maxOrder].insert((_SizeType)0); // throw(std::bad_alloc)
    }
};

//================================================================================================
// Categorizes allocations into buckets based on conversion from size to bucket index using
// functor _SizeToBucketFunc
template<class _OuterAllocator, class _SizeToBucketFunc, class _BlockType, class _SizeType = SIZE_T>
class CBucketizedBlockAllocator
{
    _OuterAllocator *m_pAllocators = nullptr;
    SIZE_T m_numAllocators = 0;

public:
    CBucketizedBlockAllocator(SIZE_T numAllocators, _OuterAllocator *pAllocators);

    _BlockType Allocate(_SizeType size);
    void Deallocate(const _BlockType &block);
    bool IsOwner(const _BlockType &block) const;
    void Reset();
};

//================================================================================================
// Uses a buddy allocator to allocate offsets from a virtual resource, with an inner allocator
// which allocates disjoint resources at a size threshold.
template<class _BlockType, class _InnerAllocator, class _SizeType = SIZE_T, _SizeType _MinBlockSize = 1>
class CDisjointBuddyAllocator
{
private:
    typedef CBuddyAllocator<_BlockType, _SizeType, _MinBlockSize> BuddyAllocatorType;
    BuddyAllocatorType m_BuddyAllocator;
    _InnerAllocator m_InnerAllocator;

    using InnerAllocatorDecayed = typename std::decay<_InnerAllocator>::type;
public:
    typedef typename std::invoke_result<decltype(&InnerAllocatorDecayed::Allocate), InnerAllocatorDecayed, _SizeType>::type AllocationType;

private:
    struct RefcountedAllocation
    {
        UINT m_Refcount = 0;
        AllocationType m_Allocation = AllocationType{};
    };
    std::vector<RefcountedAllocation> m_Allocations;

    _SizeType m_Threshold = 0;

    inline UINT BucketFromOffset(_SizeType offset) const { return UINT(offset / m_Threshold); }

public:
    template <typename... InnerAllocatorArgs>
    CDisjointBuddyAllocator(_SizeType maxBlockSize, _SizeType threshold, InnerAllocatorArgs&&... innerArgs); // throw(std::bad_alloc)
    CDisjointBuddyAllocator() = default;
    CDisjointBuddyAllocator(CDisjointBuddyAllocator&&) = default;
    CDisjointBuddyAllocator& operator=(CDisjointBuddyAllocator&&) = default;

    _BlockType Allocate(_SizeType size);
    void Deallocate(const _BlockType &block);
    bool IsOwner(const _BlockType &block) const;
    void Reset();

    AllocationType GetInnerAllocation(const _BlockType &block) const;
    _SizeType GetInnerAllocationOffset(const _BlockType &block) const;
};

//================================================================================================
// On allocate uses the _BelowOrEqualAllocator if the size is <= _ThresholdValue and the
// _AboveAllocator if the size is > _ThresholdValue 
template<class _BlockType, class _SizeType, class _BelowOrEqualAllocator, class _AboveAllocator, _SizeType _ThresholdValue>
class CThresholdAllocator
{
private:
    _BelowOrEqualAllocator m_BelowAllocator;
    _AboveAllocator m_AboveAllocator;

    using BelowDecayed = typename std::decay<_BelowOrEqualAllocator>::type;
    using AboveDecayed = typename std::decay<_AboveAllocator>::type;

public:
    template <typename Below, typename Above>
    CThresholdAllocator(Below&& below, Above&& above)
        : m_BelowAllocator(std::forward<Below>(below)), m_AboveAllocator(std::forward<Above>(above))
    {
    }

    _BlockType Allocate(_SizeType size)
    {
        if (size <= _ThresholdValue) { return m_BelowAllocator.Allocate(size); }
        else { return m_AboveAllocator.Allocate(size); }
    }
    void Deallocate(const _BlockType &block)
    {
        assert(IsOwner(block));
        if (block.GetSize() <= _ThresholdValue) { m_BelowAllocator.Deallocate(block); }
        else { m_AboveAllocator.Deallocate(block); }
    }
    bool IsOwner(const _BlockType &block) const
    {
        if (block.GetSize() <= _ThresholdValue) { return m_BelowAllocator.IsOwner(block); }
        else { return m_AboveAllocator.IsOwner(block); }
    }
    void Reset()
    {
        m_BelowAllocator.Reset();
        m_AboveAllocator.Reset();
    }

    auto GetInnerAllocation(const _BlockType &block) const
    {
        assert(IsOwner(block));
        if (block.GetSize() <= _ThresholdValue) { return m_BelowAllocator.GetInnerAllocation(block); }
        else { return m_AboveAllocator.GetInnerAllocation(block); }
    }
    _SizeType GetInnerAllocationOffset(const _BlockType &block) const
    {
        assert(IsOwner(block));
        if (block.GetSize() <= _ThresholdValue) { return m_BelowAllocator.GetInnerAllocationOffset(block); }
        else { return m_AboveAllocator.GetInnerAllocationOffset(block); }
    }
};

} // namespace BlockAllocators
