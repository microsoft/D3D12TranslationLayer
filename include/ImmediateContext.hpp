// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
class Resource;
class CommandListManager;

struct TranslationLayerCallbacks
{
    std::function<void()> m_pfnPostSubmit;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A pool of objects that are recycled on specific fence values
// This class assumes single threaded caller
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename TResourceType>
class CFencePool
{
public:
    void ReturnToPool(TResourceType&& Resource, UINT64 FenceValue) noexcept
    {
        try
        {
            auto lock = m_pLock ? std::unique_lock(*m_pLock) : std::unique_lock<std::mutex>();
            m_Pool.emplace_back(FenceValue, std::move(Resource)); // throw( bad_alloc )
        }
        catch (std::bad_alloc&)
        {
            // Just drop the error
            // All uses of this pool use unique_comptr, which will release the resource
        }
    }

    template <typename PFNCreateNew, typename... CreationArgType>
    TResourceType RetrieveFromPool(UINT64 CurrentFenceValue, PFNCreateNew pfnCreateNew, const CreationArgType&... CreationArgs) noexcept(false)
    {
        auto lock = m_pLock ? std::unique_lock(*m_pLock) : std::unique_lock<std::mutex>();
        TPool::iterator Head = m_Pool.begin();
        if (Head == m_Pool.end() || (CurrentFenceValue < Head->first))
        {
            return std::move(pfnCreateNew(CreationArgs...)); // throw( _com_error )
        }

        assert(Head->second);
        TResourceType ret = std::move(Head->second);
        m_Pool.erase(Head);
        return std::move(ret);
    }

    void Trim(UINT64 TrimThreshold, UINT64 CurrentFenceValue)
    {
        auto lock = m_pLock ? std::unique_lock(*m_pLock) : std::unique_lock<std::mutex>();

        TPool::iterator Head = m_Pool.begin();

        if (Head == m_Pool.end() || (CurrentFenceValue < Head->first))
        {
            return;
        }

        UINT64 difference = CurrentFenceValue - Head->first;

        if (difference >= TrimThreshold)
        {
            // only erase one item per 'pump'
            assert(Head->second);
            m_Pool.erase(Head);
        }
    }

    CFencePool(bool bLock = false) noexcept
        : m_pLock(bLock ? new std::mutex : nullptr)
    {
    }
    CFencePool(CFencePool &&other) noexcept
    {
        m_Pool = std::move(other.m_Pool);
        m_pLock = std::move(other.m_pLock);
    }
    CFencePool& operator=(CFencePool &&other) noexcept
    {
        m_Pool = std::move(other.m_Pool);
        m_pLock = std::move(other.m_pLock);
        return *this;
    }

protected:
    typedef std::pair<UINT64, TResourceType> TPoolEntry;
    typedef std::list<TPoolEntry> TPool;

    CFencePool(CFencePool const& other) = delete;
    CFencePool& operator=(CFencePool const& other) = delete;

protected:
    TPool m_Pool;
    std::unique_ptr<std::mutex> m_pLock;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A pool of objects that are recycled on specific fence values
// with a maximum depth before blocking on RetrieveFromPool
// This class assumes single threaded caller
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename TResourceType>
class CBoundedFencePool : public CFencePool<TResourceType>
{
public:

    template <typename PFNWaitForFenceValue, typename PFNCreateNew, typename... CreationArgType>
    TResourceType RetrieveFromPool(UINT64 CurrentFenceValue, PFNWaitForFenceValue pfnWaitForFenceValue, PFNCreateNew pfnCreateNew, const CreationArgType&... CreationArgs) noexcept(false)
    {
        auto lock = m_pLock ? std::unique_lock(*m_pLock) : std::unique_lock<std::mutex>();
        TPool::iterator Head = m_Pool.begin();

        if (Head == m_Pool.end())
        {
            return std::move(pfnCreateNew(CreationArgs...)); // throw( _com_error )
        }
        else if (CurrentFenceValue < Head->first)
        {
            if (m_Pool.size() < m_MaxInFlightDepth)
            {
                return std::move(pfnCreateNew(CreationArgs...)); // throw( _com_error )
            }
            else
            {
                pfnWaitForFenceValue(Head->first); // throw( _com_error )
            }
        }

        assert(Head->second);
        TResourceType ret = std::move(Head->second);
        m_Pool.erase(Head);
        return std::move(ret);
    }

    CBoundedFencePool(bool bLock = false, UINT MaxInFlightDepth = UINT_MAX) noexcept
        : CFencePool(bLock),
        m_MaxInFlightDepth(MaxInFlightDepth)
    {
    }
    CBoundedFencePool(CBoundedFencePool&& other) noexcept
        : CFencePool(other),
        m_MaxInFlightDepth(other.m_MaxInFlightDepth)
    {
    }
    CBoundedFencePool& operator=(CBoundedFencePool&& other) noexcept
    {
        m_Pool = std::move(other.m_Pool);
        m_pLock = std::move(other.m_pLock);
        m_MaxInFlightDepth = other.m_MaxInFlightDepth;
        return *this;
    }

protected:
    UINT m_MaxInFlightDepth;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multi-level pool (for dynamic resource data upload)
// This class is free-threaded (to enable D3D11 free-threaded resource destruction)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TResourceType, UINT64 ResourceSizeMultiple>
class CMultiLevelPool
{
public:
    CMultiLevelPool(UINT64 TrimThreshold, bool bLock)
        : m_Lock(bLock)
        , m_TrimThreshold(TrimThreshold)
    {
    }

    void ReturnToPool(UINT64 Size, TResourceType&& Resource, UINT64 FenceValue) noexcept
    {
        UINT PoolIndex = IndexFromSize(Size);
        auto Lock = m_Lock.TakeLock();

        if (PoolIndex >= m_MultiPool.size())
        {
            m_MultiPool.resize(PoolIndex + 1);
        }

        m_MultiPool[PoolIndex].ReturnToPool(std::move(Resource), FenceValue);
    }

    template <typename PFNCreateNew>
    TResourceType RetrieveFromPool(UINT64 Size, UINT64 CurrentFenceValue, PFNCreateNew pfnCreateNew) noexcept(false)
    {
        UINT PoolIndex = IndexFromSize(Size);
        UINT AlignedSize = (PoolIndex + 1) * ResourceSizeMultiple;

        auto Lock = m_Lock.TakeLock();

        if (PoolIndex >= m_MultiPool.size())
        {
            // pfnCreateNew might be expensive, and won't touch the data structure
            if (Lock.owns_lock())
            {
                Lock.unlock();
            }
            return std::move(pfnCreateNew(AlignedSize)); // throw( _com_error )
        }
        ASSUME(PoolIndex < m_MultiPool.size());

        // Note that RetrieveFromPool can call pfnCreateNew
        // m_Lock will be held during this potentially slow operation
        // This is not optimized because it is expected that once an app reaches steady-state
        // behavior, the pool will not need to grow.
        return std::move(m_MultiPool[PoolIndex].RetrieveFromPool(CurrentFenceValue, pfnCreateNew, AlignedSize)); // throw( _com_error )
    }

    void Trim(UINT64 CurrentFenceValue)
    {
        auto Lock = m_Lock.TakeLock();

        for (TPool& pool : m_MultiPool)
        {
            pool.Trim(m_TrimThreshold, CurrentFenceValue);
        }
    }

protected:
    UINT IndexFromSize(UINT64 Size) noexcept { return (Size == 0) ? 0 : (UINT)((Size - 1) / ResourceSizeMultiple); }

protected:
    typedef CFencePool<TResourceType> TPool;
    typedef std::vector<TPool> TMultiPool;

protected:
    TMultiPool m_MultiPool;
    OptLock<> m_Lock;
    UINT64 m_TrimThreshold;
};

typedef CMultiLevelPool<unique_comptr<ID3D12Resource>, 64*1024> TDynamicBufferPool;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fenced Ring Buffer
// A simple ring buffer which keeps track of allocations on the GPU time line
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CFencedRingBuffer
{
public:

    CFencedRingBuffer(UINT32 Size = 0)
        : m_Size(Size)
        , m_Head(Size)
        , m_Ledger{}
    {}

    HRESULT Allocate(UINT32 NumItems, UINT64 CurrentFenceValue, _Out_ UINT32& OffsetOut)
    {
        assert(m_Size > 0);
        assert(NumItems < m_Size / 2);

        if (NumItems == 0)
        {
            OffsetOut = DereferenceTail();
            return S_OK;
        }

        if (CurrentFenceValue > GetCurrentLedgeEntry().m_FenceValue)
        {
            if (FAILED(MoveToNextLedgerEntry(CurrentFenceValue)))
            {
                return E_FAIL;
            }
        }

        UINT64 tailLocation = DereferenceTail();

        // Allocations need to be contiguous
        if (tailLocation + NumItems > m_Size)
        {
            UINT64 remainder = m_Size - tailLocation;
            UINT32 dummy = 0;
            // Throw away the difference so we can allocate a contiguous block
            if (FAILED(Allocate(UINT32(remainder), CurrentFenceValue, dummy)))
            {
                return E_FAIL;
            }
        }

        if (m_Tail + NumItems <= m_Head)
        {
            // The tail could have moved due to alignment so deref again
            OffsetOut = DereferenceTail();
            GetCurrentLedgeEntry().m_NumAllocations += NumItems;
            m_Tail += NumItems;
            return S_OK;
        }
        else
        {
            OffsetOut = UINT32(-1);
            return E_FAIL;
        }
    }

    void Deallocate(UINT64 CompletedFenceValue)
    {
        for (size_t i = 0; i < _countof(m_Ledger); i++)
        {
            LedgerEntry& entry = m_Ledger[i];

            const UINT32 bit = (1 << i);

            if ((m_LedgerMask & bit) && entry.m_FenceValue <= CompletedFenceValue)
            {
                // Dealloc
                m_Head += entry.m_NumAllocations;
                entry = {};

                // Unset the bit
                m_LedgerMask &= ~(bit);
            }

            if (m_LedgerMask == 0)
            {
                break;
            }
        }
    }

private:

    inline UINT32 DereferenceTail() const { return m_Tail % m_Size; }

    UINT64 m_Head = 0;
    UINT64 m_Tail = 0;
    UINT32 m_Size;

    struct LedgerEntry
    {
        UINT64 m_FenceValue;
        UINT32 m_NumAllocations;
    };

    // TODO: If we define a max lag between CPU and GPU this should be set to slightly more than that
    static const UINT32 cLedgerSize = 16;

    LedgerEntry m_Ledger[cLedgerSize];
    UINT32 m_LedgerMask = 0x1;
    static_assert(cLedgerSize <= std::numeric_limits<decltype(m_LedgerMask)>::digits);

    UINT32 m_LedgerIndex = 0;

    LedgerEntry& GetCurrentLedgeEntry() { return  m_Ledger[m_LedgerIndex]; }

    bool IsLedgerEntryAvailable(UINT32 Index) const { return (m_LedgerMask & (1 << Index)) == 0; }

    HRESULT MoveToNextLedgerEntry(UINT64 CurrentFenceValue)
    {
        m_LedgerIndex++;
        m_LedgerIndex %= cLedgerSize;

        if (IsLedgerEntryAvailable(m_LedgerIndex))
        {
            m_LedgerMask |= (1 << m_LedgerIndex);

            GetCurrentLedgeEntry().m_NumAllocations = 0;
            GetCurrentLedgeEntry().m_FenceValue = CurrentFenceValue;

            return S_OK;
        }
        else
        {
            return E_FAIL;
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor heap manager
// Used to allocate descriptors from CPU-only heaps corresponding to view/sampler objects
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDescriptorHeapManager
{
public: // Types
    typedef D3D12_CPU_DESCRIPTOR_HANDLE HeapOffset;
    typedef decltype(HeapOffset::ptr) HeapOffsetRaw;
    typedef UINT HeapIndex;

private: // Types
    struct SFreeRange { HeapOffsetRaw Start; HeapOffsetRaw End; };
    struct SHeapEntry
    {
        unique_comptr<ID3D12DescriptorHeap> m_Heap;
        std::list<SFreeRange> m_FreeList;

        SHeapEntry() { }
        SHeapEntry(SHeapEntry &&o) : m_Heap(std::move(o.m_Heap)), m_FreeList(std::move(o.m_FreeList)) { }
    };

    // Note: This data structure relies on the pointer validity guarantee of std::deque,
    // that as long as inserts/deletes are only on either end of the container, pointers
    // to elements continue to be valid. If trimming becomes an option, the free heap
    // list must be re-generated at that time.
    typedef std::deque<SHeapEntry> THeapMap;

public: // Methods
    CDescriptorHeapManager(ID3D12Device* pDevice,
                           D3D12_DESCRIPTOR_HEAP_TYPE Type,
                           UINT NumDescriptorsPerHeap,
                           bool bLockRequired,
                           UINT NodeMask) noexcept
        : m_Desc( { Type,
                    NumDescriptorsPerHeap,
                    D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                    NodeMask} )
        , m_DescriptorSize(pDevice->GetDescriptorHandleIncrementSize(Type))
        , m_pDevice(pDevice)
        , m_CritSect(bLockRequired)
    {
    }

    HeapOffset AllocateHeapSlot(_Out_opt_ HeapIndex *outIndex = nullptr) noexcept(false)
    {
        auto Lock = m_CritSect.TakeLock();
        if (m_FreeHeaps.empty())
        {
            AllocateHeap(); // throw( _com_error )
        }
        assert(!m_FreeHeaps.empty());
        HeapIndex index = m_FreeHeaps.front();
        SHeapEntry &HeapEntry = m_Heaps[index];
        assert(!HeapEntry.m_FreeList.empty());
        SFreeRange &Range = *HeapEntry.m_FreeList.begin();
        HeapOffset Ret = { Range.Start };
        Range.Start += m_DescriptorSize;

        if (Range.Start == Range.End)
        {
            HeapEntry.m_FreeList.pop_front();
            if (HeapEntry.m_FreeList.empty())
            {
                m_FreeHeaps.pop_front();
            }
        }
        if (outIndex)
        {
            *outIndex = index;
        }
        return Ret;
    }

    void FreeHeapSlot(HeapOffset Offset, HeapIndex index) noexcept
    {
        auto Lock = m_CritSect.TakeLock();
        try
        {
            assert(index < m_Heaps.size());
            SHeapEntry &HeapEntry = m_Heaps[index];

            SFreeRange NewRange = 
            {
                Offset.ptr,
                Offset.ptr + m_DescriptorSize
            };

            bool bFound = false;
            for (auto it = HeapEntry.m_FreeList.begin(), end = HeapEntry.m_FreeList.end();
                 it != end && !bFound;
                 ++it)
            {
                SFreeRange &Range = *it;
                assert(Range.Start <= Range.End);
                if (Range.Start == Offset.ptr + m_DescriptorSize)
                {
                    Range.Start = Offset.ptr;
                    bFound = true;
                }
                else if (Range.End == Offset.ptr)
                {
                    Range.End += m_DescriptorSize;
                    bFound = true;
                }
                else
                {
                    assert(Range.End < Offset.ptr || Range.Start > Offset.ptr);
                    if (Range.Start > Offset.ptr)
                    {
                        HeapEntry.m_FreeList.insert(it, NewRange); // throw( bad_alloc )
                        bFound = true;
                    }
                }
            }

            if (!bFound)
            {
                if (HeapEntry.m_FreeList.empty())
                {
                    m_FreeHeaps.push_back(index); // throw( bad_alloc )
                }
                HeapEntry.m_FreeList.push_back(NewRange); // throw( bad_alloc )
            }
        }
        catch( std::bad_alloc& )
        {
            // Do nothing - there will be slots that can no longer be reclaimed.
        }
    }

private: // Methods
    void AllocateHeap() noexcept(false)
    {
        SHeapEntry NewEntry;
        ThrowFailure( m_pDevice->CreateDescriptorHeap(&m_Desc, IID_PPV_ARGS(&NewEntry.m_Heap)) ); // throw( _com_error )
        HeapOffset HeapBase = NewEntry.m_Heap->GetCPUDescriptorHandleForHeapStart();
        NewEntry.m_FreeList.push_back({HeapBase.ptr,
                                        HeapBase.ptr + m_Desc.NumDescriptors * m_DescriptorSize}); // throw( bad_alloc )

        m_Heaps.emplace_back(std::move(NewEntry)); // throw( bad_alloc )
        m_FreeHeaps.push_back(static_cast<HeapIndex>(m_Heaps.size() - 1)); // throw( bad_alloc )
    }

private: // Members
    const D3D12_DESCRIPTOR_HEAP_DESC m_Desc;
    const UINT m_DescriptorSize;
    ID3D12Device* const m_pDevice; // weak-ref
    OptLock<> m_CritSect;

    THeapMap m_Heaps;
    std::list<HeapIndex> m_FreeHeaps;
};

// Extra data appended to the end of stream-output buffers
struct SStreamOutputSuffix
{
    UINT BufferFilledSize;
    UINT VertexCountPerInstance;
    UINT InstanceCount;
    UINT StartVertexLocation;
    UINT StartInstanceLocation;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Core implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum EDirtyBits : UINT64
{
    // Pipeline states:
    // Dirty bits are set when a shader or constant in the PSO desc changes, and causes a PSO lookup/compile
    // Reassert bits are set on command list boundaries, on graphics/compute boundaries, and after dirty processing
    e_PipelineStateDirty  = 0x1,

    // Heap-based bindings:
    // Dirty bits are set when API bindings change, or on command list boundaries (to enable optimizations),
    //  and causes heap slots to be allocated, and descriptors to be copied
    // Reassert bits are set after dirty processing
    e_VSShaderResourcesDirty      = 0x4,
    e_VSConstantBuffersDirty      = 0x8,
    e_VSSamplersDirty             = 0x10,

    e_PSShaderResourcesDirty      = 0x20,
    e_PSConstantBuffersDirty      = 0x40,
    e_PSSamplersDirty             = 0x80,

    e_GSShaderResourcesDirty      = 0x100,
    e_GSConstantBuffersDirty      = 0x200,
    e_GSSamplersDirty             = 0x400,

    e_HSShaderResourcesDirty      = 0x800,
    e_HSConstantBuffersDirty      = 0x1000,
    e_HSSamplersDirty             = 0x2000,

    e_DSShaderResourcesDirty      = 0x4000,
    e_DSConstantBuffersDirty      = 0x8000,
    e_DSSamplersDirty             = 0x10000,

    e_CSShaderResourcesDirty      = 0x20000,
    e_CSConstantBuffersDirty      = 0x40000,
    e_CSSamplersDirty             = 0x80000,

    e_UnorderedAccessViewsDirty   = 0x100000,
    e_CSUnorderedAccessViewsDirty = 0x200000,

    // Non-heap-based (graphics) bindings:
    // Dirty bits are not used
    // Reassert bits are set on command list boundaries, prevent incremental bindings, and cause the full binding space to be set on PreDraw
    //  Note: incremental bindings only applies to VB and SO
    e_RenderTargetsDirty          = 0x400000,
    e_StreamOutputDirty           = 0x800000,
    e_VertexBuffersDirty          = 0x1000000,
    e_IndexBufferDirty            = 0x2000000,

    // Fixed-function knobs:
    // Dirty bits are not used
    // Reassert bits are set on command list boundaries, are unset during update DDI invocations, and cause the ImmCtx pipeline state to be applied
    e_BlendFactorDirty            = 0x4000000,
    e_StencilRefDirty             = 0x8000000,
    e_PrimitiveTopologyDirty      = 0x10000000,
    e_ViewportsDirty              = 0x20000000,
    e_ScissorRectsDirty           = 0x40000000,
    e_PredicateDirty              = 0x80000000,

    e_FirstDraw                   = 0x100000000,
    e_FirstDispatch               = 0x200000000,
    
    e_GraphicsRootSignatureDirty  = 0x400000000,
    e_ComputeRootSignatureDirty   = 0x800000000,

    // Bit combinations
    e_VSBindingsDirty             = e_VSShaderResourcesDirty | e_VSConstantBuffersDirty | e_VSSamplersDirty,
    e_PSBindingsDirty             = e_PSShaderResourcesDirty | e_PSConstantBuffersDirty | e_PSSamplersDirty,
    e_GSBindingsDirty             = e_GSShaderResourcesDirty | e_GSConstantBuffersDirty | e_GSSamplersDirty,
    e_HSBindingsDirty             = e_HSShaderResourcesDirty | e_HSConstantBuffersDirty | e_HSSamplersDirty,
    e_DSBindingsDirty             = e_DSShaderResourcesDirty | e_DSConstantBuffersDirty | e_DSSamplersDirty,

    // Combinations of Heap-based bindings, by pipeline type
    e_GraphicsBindingsDirty       = e_VSBindingsDirty | e_PSBindingsDirty | e_GSBindingsDirty | e_DSBindingsDirty | e_HSBindingsDirty | e_UnorderedAccessViewsDirty,
    e_ComputeBindingsDirty        = e_CSShaderResourcesDirty | e_CSConstantBuffersDirty | e_CSSamplersDirty | e_CSUnorderedAccessViewsDirty,

    // Combinations of heap-based bindings, by heap type
    e_ViewsDirty                  = e_VSShaderResourcesDirty | e_VSConstantBuffersDirty |
                                    e_PSShaderResourcesDirty | e_PSConstantBuffersDirty |
                                    e_GSShaderResourcesDirty | e_GSConstantBuffersDirty |
                                    e_HSShaderResourcesDirty | e_HSConstantBuffersDirty |
                                    e_DSShaderResourcesDirty | e_DSConstantBuffersDirty |
                                    e_CSShaderResourcesDirty | e_CSConstantBuffersDirty | 
                                    e_UnorderedAccessViewsDirty | e_CSUnorderedAccessViewsDirty,
    e_SamplersDirty               = e_VSSamplersDirty | e_PSSamplersDirty | e_GSSamplersDirty | e_HSSamplersDirty | e_DSSamplersDirty | e_CSSamplersDirty,

    // All heap-based bindings
    e_HeapBindingsDirty           = e_GraphicsBindingsDirty | e_ComputeBindingsDirty,

    // (Mostly) graphics-only bits, except for predicate
    e_NonHeapBindingsDirty        = e_RenderTargetsDirty | e_StreamOutputDirty | e_VertexBuffersDirty | e_IndexBufferDirty,
    e_FixedFunctionDirty          = e_BlendFactorDirty | e_StencilRefDirty | e_PrimitiveTopologyDirty | e_ViewportsDirty | e_ScissorRectsDirty | e_PredicateDirty,

    // All state bits by pipeline type
    e_GraphicsStateDirty          = e_PipelineStateDirty | e_GraphicsBindingsDirty | e_NonHeapBindingsDirty | e_FixedFunctionDirty | e_FirstDraw | e_GraphicsRootSignatureDirty,
    e_ComputeStateDirty           = e_PipelineStateDirty | e_ComputeBindingsDirty | e_FirstDispatch | e_ComputeRootSignatureDirty,

    // Accumulations of state bits set on command list boundaries and initialization
    // New command lists require all state to be reasserted, but nothing new needs to be dirtied.  
    // The first command list associated with a device must treat all heaps as dirty
    // to setup initial descriptor tables
    e_DirtyOnNewCommandList       = 0,
    e_DirtyOnFirstCommandList     = e_HeapBindingsDirty,
    e_ReassertOnNewCommandList    = e_GraphicsStateDirty | e_ComputeStateDirty,
};

class ImmediateContext;

struct RetiredObject
{
    RetiredObject() {}
    RetiredObject(COMMAND_LIST_TYPE CommandListType, UINT64 lastCommandListID, bool completionRequired, std::vector<DeferredWait> deferredWaits = std::vector<DeferredWait>()) :
        m_completionRequired(completionRequired),
        m_deferredWaits(std::move(deferredWaits))
    {
        m_lastCommandListIDs[(UINT)CommandListType] = lastCommandListID;
    }

    RetiredObject(const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired, std::vector<DeferredWait> deferredWaits = std::vector<DeferredWait>()) :
        m_completionRequired(completionRequired),
        m_deferredWaits(std::move(deferredWaits))
    {
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
        {
            m_lastCommandListIDs[i] = lastCommandListIDs[i];
        }
    }

    static bool ReadyToDestroy(ImmediateContext* pContext, bool completionRequired, UINT64 lastCommandListID, COMMAND_LIST_TYPE CommandListType, const std::vector<DeferredWait>& deferredWaits = std::vector<DeferredWait>());
    static bool ReadyToDestroy(ImmediateContext* pContext, bool completionRequired, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], const std::vector<DeferredWait>& deferredWaits = std::vector<DeferredWait>());
    static bool DeferredWaitsSatisfied(const std::vector<DeferredWait>& deferredWaits);
    bool ReadyToDestroy(ImmediateContext* pContext) { return ReadyToDestroy(pContext, m_completionRequired, m_lastCommandListIDs, m_deferredWaits); }

    UINT64 m_lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
    bool m_completionRequired = false;
    std::vector<DeferredWait> m_deferredWaits;
};

struct RetiredD3D12Object : public RetiredObject
{
    RetiredD3D12Object() {}
    RetiredD3D12Object(ID3D12Object* pUnderlying, _In_opt_ std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, COMMAND_LIST_TYPE CommandListType, UINT64 lastCommandListID, bool completionRequired, std::vector<DeferredWait> deferredWaits) :
        RetiredObject(CommandListType, lastCommandListID, completionRequired, std::move(deferredWaits))
        , m_pUnderlying(pUnderlying)
        , m_pResidencyHandle(std::move(pResidencyHandle)) {}

    RetiredD3D12Object(ID3D12Object* pUnderlying, _In_opt_ std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired, std::vector<DeferredWait> deferredWaits) :
        RetiredObject(lastCommandListIDs, completionRequired, std::move(deferredWaits))
        , m_pUnderlying(pUnderlying)
        , m_pResidencyHandle(std::move(pResidencyHandle)) {}

    RetiredD3D12Object(RetiredD3D12Object &&retiredObject) :
        RetiredObject(retiredObject)
        , m_pUnderlying(retiredObject.m_pUnderlying)
        , m_pResidencyHandle(std::move(retiredObject.m_pResidencyHandle)) {}
    

    CComPtr<ID3D12Object> m_pUnderlying;
    

    std::unique_ptr<ResidencyManagedObjectWrapper> m_pResidencyHandle;
};

typedef ConditionalAllocator<HeapSuballocationBlock, UINT64, DirectHeapAllocator, ThreadSafeBuddyHeapAllocator, bool> ConditionalHeapAllocator;
struct RetiredSuballocationBlock : public RetiredObject
{
    RetiredSuballocationBlock(HeapSuballocationBlock &block, ConditionalHeapAllocator &parentAllocator, COMMAND_LIST_TYPE CommandListType, UINT64 lastCommandListID) :
        RetiredObject(CommandListType, lastCommandListID, true)
        , m_SuballocatedBlock(block)
        , m_ParentAllocator(parentAllocator) {}

    RetiredSuballocationBlock(HeapSuballocationBlock &block, ConditionalHeapAllocator &parentAllocator, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID]) :
        RetiredObject(lastCommandListIDs, true)
        , m_SuballocatedBlock(block)
        , m_ParentAllocator(parentAllocator) {}

    void Destroy()
    {
        m_ParentAllocator.Deallocate(m_SuballocatedBlock);
    }

    HeapSuballocationBlock m_SuballocatedBlock;
    ConditionalHeapAllocator &m_ParentAllocator;
};

class DeferredDeletionQueueManager
{
public:
    DeferredDeletionQueueManager(ImmediateContext *pContext)
        : m_pParent(pContext)
    {}

    ~DeferredDeletionQueueManager() {
        TrimDeletedObjects(true);
    }

    bool TrimDeletedObjects(bool deviceBeingDestroyed = false);
    bool GetFenceValuesForObjectDeletion(UINT64(&FenceValues)[(UINT)COMMAND_LIST_TYPE::MAX_VALID]);
    bool GetFenceValuesForSuballocationDeletion(UINT64(&FenceValues)[(UINT)COMMAND_LIST_TYPE::MAX_VALID]);

    void AddObjectToQueue(ID3D12Object* pUnderlying, std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, COMMAND_LIST_TYPE CommandListType, UINT64 lastCommandListID, bool completionRequired, std::vector<DeferredWait> deferredWaits = std::vector<DeferredWait>())
    {
        m_DeferredObjectDeletionQueue.push(RetiredD3D12Object(pUnderlying, std::move(pResidencyHandle), CommandListType, lastCommandListID, completionRequired, std::move(deferredWaits)));
    }

    void AddObjectToQueue(ID3D12Object* pUnderlying, std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired, std::vector<DeferredWait> deferredWaits = std::vector<DeferredWait>())
    {
        m_DeferredObjectDeletionQueue.push(RetiredD3D12Object(pUnderlying, std::move(pResidencyHandle), lastCommandListIDs, completionRequired, std::move(deferredWaits)));
    }

    void AddSuballocationToQueue(HeapSuballocationBlock &suballocation, ConditionalHeapAllocator &parentAllocator, COMMAND_LIST_TYPE CommandListType, UINT64 lastCommandListID)
    {
        RetiredSuballocationBlock retiredSuballocation(suballocation, parentAllocator, CommandListType, lastCommandListID);
        if (!retiredSuballocation.ReadyToDestroy(m_pParent))
        {
            m_DeferredSuballocationDeletionQueue.push(retiredSuballocation);
        }
        else
        {
            retiredSuballocation.Destroy();
        }
    }

    void AddSuballocationToQueue(HeapSuballocationBlock &suballocation, ConditionalHeapAllocator &parentAllocator, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID])
    {
        RetiredSuballocationBlock retiredSuballocation(suballocation, parentAllocator, lastCommandListIDs);
        if (!retiredSuballocation.ReadyToDestroy(m_pParent))
        {
            m_DeferredSuballocationDeletionQueue.push(retiredSuballocation);
        }
        else
        {
            retiredSuballocation.Destroy();
        }
    }

private:
    bool SuballocationsReadyToBeDestroyed(bool deviceBeingDestroyed);

    ImmediateContext* m_pParent;
    std::queue<RetiredD3D12Object> m_DeferredObjectDeletionQueue;
    std::queue<RetiredSuballocationBlock> m_DeferredSuballocationDeletionQueue;
};

template <typename T, typename mutex_t = std::mutex> class COptLockedContainer
{
    OptLock<mutex_t> m_CS;
    T m_Obj;
public:
    class LockedAccess
    {
        std::unique_lock<mutex_t> m_Lock;
        T& m_Obj;
    public:
        LockedAccess(OptLock<mutex_t> &CS, T& Obj)
            : m_Lock(CS.TakeLock())
            , m_Obj(Obj) { }
        T* operator->() { return &m_Obj; }
    };
    // Intended use: GetLocked()->member.
    // The LockedAccess temporary object ensures synchronization until the end of the expression.
    template <typename... Args> COptLockedContainer(Args&&... args) : m_Obj(std::forward<Args>(args)...) { }
    LockedAccess GetLocked() { return LockedAccess(m_CS, m_Obj); }
    void InitLock() { m_CS.EnsureLock(); }
};

enum ResourceInfoType
{
    TiledPoolType,
    ResourceType
};

struct ResourceInfo
{
    union
    {
        struct
        {
            D3D12_HEAP_DESC m_HeapDesc;
        } TiledPool;

        struct {
            D3D12_RESOURCE_DESC m_ResourceDesc;
            D3D11_RESOURCE_FLAGS m_Flags11;
            D3D12_HEAP_FLAGS m_HeapFlags;
            D3D12_HEAP_PROPERTIES m_HeapProps;
        } Resource;
    };
    ResourceInfoType m_Type;
    bool m_bShared;
    bool m_bNTHandle;
    bool m_bSynchronized;
    bool m_bAllocatedBy9on12;
    HANDLE m_GDIHandle;
};

using RenameResourceSet = std::deque<unique_comptr<Resource>>;

struct PresentSurface
{
    PresentSurface() : m_pResource(nullptr), m_subresource(0) {}
    PresentSurface(Resource* pResource, UINT subresource = 0) : m_pResource(pResource), m_subresource(subresource) {}

    Resource* m_pResource;
    UINT m_subresource;
};

struct PresentCBArgs
{
    _In_ ID3D12CommandQueue* pGraphicsCommandQueue;
    _In_ ID3D12CommandList* pGraphicsCommandList;
    _In_reads_(numSrcSurfaces) const PresentSurface* pSrcSurfaces;
    UINT numSrcSurfaces;
    _In_opt_ Resource* pDest;
    UINT flipInterval;
    UINT vidPnSourceId;
    _In_ D3DKMT_PRESENT* pKMTPresent;
};

class ImmediateContext
{
public:
    // D3D12 objects
    // TODO: const
    const unique_comptr<ID3D12Device> m_pDevice12;
#if DYNAMIC_LOAD_DXCORE
    XPlatHelpers::unique_module m_DXCore;
#endif
    unique_comptr<IDXCoreAdapter> m_pDXCoreAdapter;
    unique_comptr<IDXGIAdapter3> m_pDXGIAdapter;
    unique_comptr<ID3D12Device1> m_pDevice12_1;
    unique_comptr<ID3D12Device2> m_pDevice12_2; // TODO: Instead of adding more next time, replace
    unique_comptr<ID3D12CompatibilityDevice> m_pCompatDevice;
    unique_comptr<ID3D12CommandQueue> m_pSyncOnlyQueue;
private:
    std::unique_ptr<CommandListManager> m_CommandLists[(UINT)COMMAND_LIST_TYPE::MAX_VALID];

    // Residency Manager needs to come after the deferred deletion queue so that defer deleted objects can
    // call EndTrackingObject on a valid residency manager
    ResidencyManager m_residencyManager;

    // It is important that the deferred deletion queue manager gets destroyed last, place solely strict dependencies above.
    COptLockedContainer<DeferredDeletionQueueManager> m_DeferredDeletionQueueManager;

    // Must be initialized before BindingTracker logic for m_CurrentState
    D3D_FEATURE_LEVEL m_FeatureLevel;
public:

    class CDisablePredication
    {
    public:
        CDisablePredication(ImmediateContext* pParent);
        ~CDisablePredication();

    private:
        ImmediateContext* m_pParent;
    };

    friend class Query;
    friend class CommandListManager;

    class CreationArgs
    {
    public:
        CreationArgs() { ZeroMemory(this, sizeof(*this)); }
        
        UINT RequiresBufferOutOfBoundsHandling : 1;
        UINT CreatesAndDestroysAreMultithreaded : 1;
        UINT RenamingIsMultithreaded : 1;
        UINT UseThreadpoolForPSOCreates : 1;
        UINT UseRoundTripPSOs : 1;
        UINT UseResidencyManagement : 1;
        UINT DisableGPUTimeout : 1;
        UINT IsXbox : 1;
        UINT AdjustYUY2BlitCoords : 1;
        GUID CreatorID;
        DWORD MaxAllocatedUploadHeapSpacePerCommandList;
        DWORD MaxSRVHeapSize;
    };

    ImmediateContext(UINT nodeIndex, D3D12_FEATURE_DATA_D3D12_OPTIONS& caps,
        ID3D12Device* pDevice, ID3D12CommandQueue* pQueue, TranslationLayerCallbacks const& callbacks, UINT64 debugFlags, CreationArgs args) noexcept(false);
    ~ImmediateContext() noexcept;

#if TRANSLATION_LAYER_DBG
    UINT64 DebugFlags() { return m_DebugFlags; }
#endif
    CreationArgs m_CreationArgs;

    bool RequiresBufferOutofBoundsHandling() { return m_CreationArgs.RequiresBufferOutOfBoundsHandling; }
    bool IsXbox() { return m_CreationArgs.IsXbox; } // Currently only accurate with D3D11.

    CommandListManager *GetCommandListManager(COMMAND_LIST_TYPE type) noexcept;
    ID3D12CommandList *GetCommandList(COMMAND_LIST_TYPE type) noexcept;
    UINT64 GetCommandListID(COMMAND_LIST_TYPE type) noexcept;
    UINT64 GetCommandListIDInterlockedRead(COMMAND_LIST_TYPE type) noexcept;
    UINT64 GetCommandListIDWithCommands(COMMAND_LIST_TYPE type) noexcept;
    UINT64 GetCompletedFenceValue(COMMAND_LIST_TYPE type) noexcept;
    ID3D12CommandQueue *GetCommandQueue(COMMAND_LIST_TYPE type) noexcept;
    void ResetCommandList(UINT commandListTypeMask) noexcept;
    void CloseCommandList(UINT commandListTypeMask) noexcept;
    HRESULT EnqueueSetEvent(UINT commandListTypeMask, HANDLE hEvent) noexcept;
    HRESULT EnqueueSetEvent(COMMAND_LIST_TYPE commandListType, HANDLE hEvent) noexcept;
    Fence *GetFence(COMMAND_LIST_TYPE type) noexcept;
    void SubmitCommandList(UINT commandListTypeMask);
    void SubmitCommandList(COMMAND_LIST_TYPE commandListType);

    // Returns true if synchronization was successful, false likely means device is removed
    bool WaitForCompletion(UINT commandListTypeMask) noexcept;
    bool WaitForCompletion(COMMAND_LIST_TYPE commandListType);
    bool WaitForFenceValue(COMMAND_LIST_TYPE commandListType, UINT64 FenceValue);
    bool WaitForFenceValue(COMMAND_LIST_TYPE type, UINT64 FenceValue, bool DoNotWait);

    ID3D12GraphicsCommandList *GetGraphicsCommandList() noexcept;
    ID3D12VideoDecodeCommandList2 *GetVideoDecodeCommandList() noexcept;
    ID3D12VideoProcessCommandList2 *GetVideoProcessCommandList() noexcept;
    void AdditionalCommandsAdded(COMMAND_LIST_TYPE type) noexcept;
    void UploadHeapSpaceAllocated(COMMAND_LIST_TYPE type, UINT64 HeapSize) noexcept;

    unique_comptr<ID3D12Resource> AllocateHeap(UINT64 HeapSize, UINT64 alignment, AllocatorHeapType heapType) noexcept(false);

    void InitializeVideo(ID3D12VideoDevice **ppVideoDevice);

    void AddObjectToResidencySet(Resource *pResource, COMMAND_LIST_TYPE commandListType);
    void AddResourceToDeferredDeletionQueue(ID3D12Object* pUnderlying, std::unique_ptr<ResidencyManagedObjectWrapper> &&pResidencyHandle, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired, std::vector<DeferredWait> deferredWaits);
    void AddObjectToDeferredDeletionQueue(ID3D12Object* pUnderlying, COMMAND_LIST_TYPE commandListType, UINT64 lastCommandListID, bool completionRequired);
    void AddObjectToDeferredDeletionQueue(ID3D12Object* pUnderlying, const UINT64 lastCommandListIDs[(UINT)COMMAND_LIST_TYPE::MAX_VALID], bool completionRequired);

    bool TrimDeletedObjects(bool deviceBeingDestroyed = false);
    bool TrimResourcePools();

    unique_comptr<ID3D12Resource> AcquireTransitionableUploadBuffer(AllocatorHeapType HeapType, UINT64 Size) noexcept(false);

    void ReturnTransitionableBufferToPool(AllocatorHeapType HeapType, UINT64 Size, unique_comptr<ID3D12Resource>&&spResource, UINT64 FenceValue) noexcept;

    D3D12ResourceSuballocation AcquireSuballocatedHeapForResource(_In_ Resource* pResource, ResourceAllocationContext threadingContext) noexcept(false);
    D3D12ResourceSuballocation AcquireSuballocatedHeap(AllocatorHeapType HeapType, UINT64 Size, ResourceAllocationContext threadingContext, bool bCannotBeOffset = false) noexcept(false);
    void ReleaseSuballocatedHeap(AllocatorHeapType HeapType, D3D12ResourceSuballocation &resource, UINT64 FenceValue, COMMAND_LIST_TYPE commandListType) noexcept;
    void ReleaseSuballocatedHeap(AllocatorHeapType HeapType, D3D12ResourceSuballocation &resource, const UINT64 FenceValues[]) noexcept;

    void ReturnAllBuffersToPool( Resource& UnderlyingResource) noexcept;
   

    static void UploadDataToMappedBuffer(_In_reads_bytes_(Placement.Depth * DepthPitch) const void* pData, UINT SrcPitch, UINT SrcDepth, 
                                         _Out_writes_bytes_(Placement.Depth * DepthPitch) void* pMappedData,
                                         D3D12_SUBRESOURCE_FOOTPRINT& Placement, UINT DepthPitch, UINT TightRowPitch) noexcept;

    // This is similar to the D3D12 header helper method, but it can handle 11on12-emulated resources, as well as a dst box
    enum class UpdateSubresourcesFlags
    {
        ScenarioImmediateContext,           // Servicing an immediate context operation, e.g. UpdateSubresource API or some kind of clear
        ScenarioInitialData,                // Servicing a free-threaded method, but guaranteed that the dest resource is idle
        ScenarioBatchedContext,             // Servicing a queued operation, but may be occurring in parallel with immediate context operations
        ScenarioImmediateContextInternalOp, // Servicing an internal immediate context operation (e.g. updating UAV/SO counters) and should not respect predication
        ScenarioMask = 0x3,

        None = 0,
        ChannelSwapR10G10B10A2 = 0x4,
    };
    void UpdateSubresources(Resource* pDst,
                            D3D12TranslationLayer::CSubresourceSubset const& Subresources,
                            _In_reads_opt_(_Inexpressible_(Subresources.NumNonExtendedSubresources())) const D3D11_SUBRESOURCE_DATA* pSrcData,
                            _In_opt_ const D3D12_BOX* pDstBox = nullptr,
                            UpdateSubresourcesFlags flags = UpdateSubresourcesFlags::ScenarioImmediateContext,
                            _In_opt_ const void* pClearColor = nullptr );

    struct PreparedUpdateSubresourcesOperation
    {
        UINT64 OffsetAdjustment;                     // 0-8 bytes
        EncodedResourceSuballocation EncodedBlock;   // 8-32 bytes (last 4 bytes padding on x86)
        CSubresourceSubset EncodedSubresourceSubset; // 32-40 bytes
        UINT DstX;                                   // 40-44 bytes
        UINT DstY;                                   // 44-48 bytes
        UINT DstZ;                                   // 48-52 bytes
        bool bDisablePredication;                    // byte 52
        bool bDstBoxPresent;                         // byte 53
        // 2 bytes padding
    };
    static_assert(sizeof(PreparedUpdateSubresourcesOperation) == 56, "Math above is wrong. Check if padding can be removed.");
    struct PreparedUpdateSubresourcesOperationWithLocalPlacement
    {
        PreparedUpdateSubresourcesOperation Base;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT LocalPlacementDescs[2];
    };

    class CPrepareUpdateSubresourcesHelper
    {
    public:
        PreparedUpdateSubresourcesOperationWithLocalPlacement PreparedStorage;
        Resource& Dst;
        CSubresourceSubset const& Subresources;

        const bool bDeInterleavingUpload = Dst.SubresourceMultiplier() > 1;
        const UINT NumSrcData = Subresources.NumNonExtendedSubresources();
        const UINT NumDstSubresources = Subresources.NumExtendedSubresources();

        const UINT8 PlaneCount = (Dst.SubresourceMultiplier() * Dst.AppDesc()->NonOpaquePlaneCount());
        const UINT FirstDstSubresource = ComposeSubresourceIdxExtended(Subresources.m_BeginMip, Subresources.m_BeginArray, Subresources.m_BeginPlane, Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize());
        const UINT LastDstSubresource = ComposeSubresourceIdxExtended(Subresources.m_EndMip - 1, Subresources.m_EndArray - 1, Subresources.m_EndPlane - 1, Dst.AppDesc()->MipLevels(), Dst.AppDesc()->ArraySize());

        const bool bDisjointSubresources = LastDstSubresource - FirstDstSubresource + 1 != NumDstSubresources;
        const bool bDstBoxPresent;
        const bool bUseLocalPlacement = bDstBoxPresent || bDisjointSubresources;

        bool FinalizeNeeded = false;

    private:
        UINT64 TotalSize = 0;
        D3D12ResourceSuballocation mappableResource;
        UINT bufferOffset = 0;
        bool CachedNeedsTemporaryUploadHeap = false;

    public:
        CPrepareUpdateSubresourcesHelper(Resource& Dst,
                                         CSubresourceSubset const& Subresources,
                                         const D3D11_SUBRESOURCE_DATA* pSrcData,
                                         const D3D12_BOX* pDstBox,
                                         UpdateSubresourcesFlags flags,
                                         const void* pClearPattern,
                                         UINT ClearPatternSize,
                                         ImmediateContext& ImmCtx);

    private:
#if TRANSLATION_LAYER_DBG
        void AssertPreconditions(const D3D11_SUBRESOURCE_DATA* pSrcData, const void* pClearPattern);
#endif

        bool InitializePlacementsAndCalculateSize(const D3D12_BOX* pDstBox, ID3D12Device* pDevice);
        bool NeedToRespectPredication(UpdateSubresourcesFlags flags) const;
        bool NeedTemporaryUploadHeap(UpdateSubresourcesFlags flags, ImmediateContext& ImmCtx) const;
        void InitializeMappableResource(UpdateSubresourcesFlags flags, ImmediateContext& ImmCtx, D3D12_BOX const* pDstBox);
        void UploadSourceDataToMappableResource(void* pDstData, D3D11_SUBRESOURCE_DATA const* pSrcData, ImmediateContext& ImmCtx, UpdateSubresourcesFlags flags);
        void UploadDataToMappableResource(D3D11_SUBRESOURCE_DATA const* pSrcData, ImmediateContext& ImmCtx, D3D12_BOX const* pDstBox, const void* pClearPattern, UINT ClearPatternSize, UpdateSubresourcesFlags flags);
        void WriteOutputParameters(D3D12_BOX const* pDstBox, UpdateSubresourcesFlags flags);
    };
    void FinalizeUpdateSubresources(Resource* pDst, PreparedUpdateSubresourcesOperation const& PreparedStorage, _In_reads_opt_(2) D3D12_PLACED_SUBRESOURCE_FOOTPRINT const* LocalPlacementDescs);

    void CopyAndConvertSubresourceRegion(Resource* pDst, UINT DstSubresource, Resource* pSrc, UINT SrcSubresource, UINT dstX, UINT dstY, UINT dstZ, const D3D12_BOX* pSrcBox) noexcept;
    bool CreatesAndDestroysAreMultithreaded() const noexcept { return m_CreationArgs.CreatesAndDestroysAreMultithreaded; }

    void UAVBarrier() noexcept;

    // Mark resources and add them to the transition list
    // Subresource binding states are assumed to already be changed using wrappers below
    void TransitionResourceForBindings(Resource* pResource) noexcept;
    void TransitionResourceForBindings(ViewBase* pView) noexcept;
    static void ConstantBufferBound(Resource* pBuffer, UINT slot, EShaderStage stage) noexcept;
    static void ConstantBufferUnbound(Resource* pBuffer, UINT slot, EShaderStage stage) noexcept;
    static void VertexBufferBound(Resource* pBuffer, UINT slot) noexcept;
    static void VertexBufferUnbound(Resource* pBuffer, UINT slot) noexcept;
    static void IndexBufferBound(Resource* pBuffer) noexcept;
    static void IndexBufferUnbound(Resource* pBuffer) noexcept;
    static void StreamOutputBufferBound(Resource* pBuffer, UINT slot) noexcept;
    static void StreamOutputBufferUnbound(Resource* pBuffer, UINT slot) noexcept;

    void ClearDSVBinding();
    void ClearRTVBinding(UINT slot);
    void ClearVBBinding(UINT slot);

    void SetPredicationInternal(Query*, BOOL);

    void WriteToSubresource(Resource* DstResource, UINT DstSubresource, _In_opt_ const D3D11_BOX* pDstBox, 
                            const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
    void ReadFromSubresource(void* pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                             Resource* SrcResource, UINT SrcSubresource, _In_opt_ const D3D11_BOX* pSrcBox);

    ResourceCache &GetResourceCache() { return m_ResourceCache; }

public:
    PipelineState* GetPipelineState();
    void TRANSLATION_API SetPipelineState(PipelineState* pPipeline);

    void TRANSLATION_API Draw(UINT, UINT );
    void TRANSLATION_API DrawInstanced(UINT, UINT, UINT, UINT);

    void TRANSLATION_API DrawIndexed(UINT, UINT, INT );
    void TRANSLATION_API DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT );
    void TRANSLATION_API DrawAuto();
    void TRANSLATION_API DrawIndexedInstancedIndirect(Resource*, UINT );
    void TRANSLATION_API DrawInstancedIndirect(Resource*, UINT );
    
    void TRANSLATION_API Dispatch( UINT, UINT, UINT );
    void TRANSLATION_API DispatchIndirect(Resource*, UINT );

    // Returns if any work was actually submitted
    bool TRANSLATION_API Flush(UINT commandListMask);

    void TRANSLATION_API IaSetTopology(D3D12_PRIMITIVE_TOPOLOGY);
    void TRANSLATION_API IaSetVertexBuffers( UINT, __in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT, Resource* const*, const UINT*, const UINT* );
    void TRANSLATION_API IaSetIndexBuffer( Resource*, DXGI_FORMAT, UINT );

    template<EShaderStage eShader>
    void TRANSLATION_API SetShaderResources( UINT, __in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) UINT, SRV* const* );
    template<EShaderStage eShader>
    void TRANSLATION_API SetSamplers( UINT, __in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) UINT, Sampler* const* );
    template<EShaderStage eShader>
    void TRANSLATION_API SetConstantBuffers( UINT, __in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT) UINT Buffers, Resource* const*, __in_ecount_opt(Buffers) CONST UINT* pFirstConstant, __in_ecount_opt(Buffers) CONST UINT* pNumConstants);

    void TRANSLATION_API SoSetTargets(_In_range_(0, 4) UINT NumTargets, _In_range_(0, 4) UINT, _In_reads_(NumTargets) Resource* const*, _In_reads_(NumTargets) const UINT* );
    void TRANSLATION_API OMSetRenderTargets(__in_ecount(NumRTVs) RTV* const* pRTVs, __in_range(0, 8) UINT NumRTVs, __in_opt DSV *);
    void TRANSLATION_API OMSetUnorderedAccessViews(UINT, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV* const*, __in_ecount(NumViews) CONST UINT* );
    void TRANSLATION_API CsSetUnorderedAccessViews(UINT, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV* const*, __in_ecount(NumViews) CONST UINT* );

    void TRANSLATION_API OMSetStencilRef( UINT );
    void TRANSLATION_API OMSetBlendFactor(const FLOAT[4]);

    void TRANSLATION_API SetViewport(UINT slot, const D3D12_VIEWPORT*);
    void TRANSLATION_API SetNumViewports(UINT num);
    void TRANSLATION_API SetScissorRect(UINT slot, const D3D12_RECT*);
    void TRANSLATION_API SetNumScissorRects(UINT num);
    void TRANSLATION_API SetScissorRectEnable(BOOL);

    void TRANSLATION_API ClearRenderTargetView(RTV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearDepthStencilView(DSV *, UINT, FLOAT, UINT8, UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearUnorderedAccessViewUint(UAV *, CONST UINT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearUnorderedAccessViewFloat(UAV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearResourceWithNoRenderTarget(Resource* pResource, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects, UINT Subresource, UINT BaseSubresource, DXGI_FORMAT ClearFormat);
    void TRANSLATION_API ClearVideoDecoderOutputView(VDOV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearVideoProcessorInputView(VPIV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);
    void TRANSLATION_API ClearVideoProcessorOutputView(VPOV *, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);

    void TRANSLATION_API DiscardView(ViewBase* pView, const D3D12_RECT*, UINT);
    void TRANSLATION_API DiscardResource(Resource* pResource, const D3D12_RECT*, UINT);

    void TRANSLATION_API GenMips(SRV *, D3D12_FILTER_TYPE FilterType);

    void TRANSLATION_API QueryBegin( Async* );
    void TRANSLATION_API QueryEnd(Async*);
    bool TRANSLATION_API QueryGetData(Async*, void*, UINT, bool DoNotFlush, bool AsyncGetData = false);
    void TRANSLATION_API SetPredication(Query*, BOOL);

    bool TRANSLATION_API Map(_In_ Resource* pResource, _In_ UINT Subresource, _In_ MAP_TYPE MapType, _In_ bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, _Out_ MappedSubresource* pMappedSubresource);
    void TRANSLATION_API Unmap(Resource*, UINT, MAP_TYPE, _In_opt_ const D3D12_BOX *pReadWriteRange);

    bool SynchronizeForMap(Resource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait);
    bool TRANSLATION_API MapUnderlying(Resource*, UINT, MAP_TYPE, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* );
    bool TRANSLATION_API MapUnderlyingSynchronize(Resource*, UINT, MAP_TYPE, bool, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* );

    bool TRANSLATION_API MapDiscardBuffer( Resource* pResource, UINT Subresource, MAP_TYPE, bool, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* );
    bool TRANSLATION_API MapDynamicTexture( Resource* pResource, UINT Subresource, MAP_TYPE, bool, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* );
    bool TRANSLATION_API MapDefault(Resource*pResource, UINT Subresource, MAP_TYPE, bool doNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource*);
    void TRANSLATION_API UnmapDefault( Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    void TRANSLATION_API UnmapUnderlyingSimple( Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    void TRANSLATION_API UnmapUnderlyingStaging( Resource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange);
    void TRANSLATION_API UnmapDynamicTexture( Resource*pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange, bool bUploadMappedContents);

    Resource* TRANSLATION_API CreateRenameCookie(Resource* pResource, ResourceAllocationContext threadingContext);
    void TRANSLATION_API Rename(Resource* pResource, Resource* pRenameResource);
    void TRANSLATION_API RenameViaCopy(Resource* pResource, Resource* pRenameResource, UINT DirtyPlaneMask);
    void TRANSLATION_API DeleteRenameCookie(Resource* pRenameResource);

    void TRANSLATION_API ClearInputBindings(Resource* pResource);
    void TRANSLATION_API ClearOutputBindings(Resource* pResource);

    void TRANSLATION_API ResourceCopy( Resource*, Resource* );
    void TRANSLATION_API ResourceResolveSubresource( Resource*, UINT, Resource*, UINT, DXGI_FORMAT );

    void TRANSLATION_API SetResourceMinLOD( Resource*, FLOAT );

    void TRANSLATION_API CopyStructureCount( Resource*, UINT, UAV* );
    
    void TRANSLATION_API ResourceCopyRegion( Resource*, UINT, UINT, UINT, UINT, Resource*, UINT, const D3D12_BOX*);
    void TRANSLATION_API ResourceUpdateSubresourceUP( Resource*, UINT, _In_opt_ const D3D12_BOX*, _In_ const VOID*, UINT, UINT);

    HRESULT TRANSLATION_API GetDeviceState();


    enum TILE_MAPPING_FLAG
    {
        TILE_MAPPING_NO_OVERWRITE = 0x00000001,
    };

    enum TILE_RANGE_FLAG
    {
        TILE_RANGE_NULL = 0x00000001,
        TILE_RANGE_SKIP = 0x00000002,
        TILE_RANGE_REUSE_SINGLE_TILE = 0x00000004,
    };

    enum TILE_COPY_FLAG
    {
        TILE_COPY_NO_OVERWRITE = 0x00000001,
        TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE = 0x00000002,
        TILE_COPY_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER = 0x00000004,
    };

    void TRANSLATION_API UpdateTileMappings(Resource* hTiledResource, UINT NumTiledResourceRegions,_In_reads_(NumTiledResourceRegions) 
        const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords,_In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
        Resource* hTilePool,UINT NumRanges,_In_reads_opt_(NumRanges) const TILE_RANGE_FLAG* pRangeFlags, _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets, _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts, TILE_MAPPING_FLAG Flags );

    void TRANSLATION_API CopyTileMappings(Resource*, _In_ const D3D12_TILED_RESOURCE_COORDINATE*, Resource*, _In_ const  D3D12_TILED_RESOURCE_COORDINATE*, _In_ const D3D12_TILE_REGION_SIZE*, TILE_MAPPING_FLAG);
    void TRANSLATION_API CopyTiles(Resource*, _In_ const D3D12_TILED_RESOURCE_COORDINATE*, _In_ const D3D12_TILE_REGION_SIZE*, Resource*, UINT64, TILE_COPY_FLAG);
    void TRANSLATION_API UpdateTiles(Resource*, _In_ const D3D12_TILED_RESOURCE_COORDINATE*, _In_ const D3D12_TILE_REGION_SIZE*, const _In_ VOID*, UINT);
    void TRANSLATION_API TiledResourceBarrier(Resource* pBefore, Resource* pAfter);
    void TRANSLATION_API ResizeTilePool(Resource*, UINT64);
    void TRANSLATION_API GetMipPacking(Resource*, _Out_ UINT*, _Out_ UINT*);

    HRESULT TRANSLATION_API CheckFormatSupport(_Out_ D3D12_FEATURE_DATA_FORMAT_SUPPORT& formatData);
    bool SupportsRenderTarget(DXGI_FORMAT Format);
    void TRANSLATION_API CheckMultisampleQualityLevels(DXGI_FORMAT, UINT, D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS, _Out_ UINT*);
    void TRANSLATION_API CheckFeatureSupport(D3D12_FEATURE Feature, _Inout_updates_bytes_(FeatureSupportDataSize)void* pFeatureSupportData, UINT FeatureSupportDataSize);

    void TRANSLATION_API SetHardwareProtection(Resource*, INT);
    void TRANSLATION_API SetHardwareProtectionState(BOOL);
    
    void TRANSLATION_API Signal(_In_ Fence* pFence, UINT64 Value);
    void TRANSLATION_API Wait(std::shared_ptr<Fence> const& pFence, UINT64 Value);

    void TRANSLATION_API RotateResourceIdentities(Resource* const* ppResources, UINT Resources);
    HRESULT TRANSLATION_API ResolveSharedResource(Resource* pResource);

    void TRANSLATION_API ClearState();

    void TRANSLATION_API SetMarker(const wchar_t* name);
    void TRANSLATION_API BeginEvent(const wchar_t* name);
    void TRANSLATION_API EndEvent();
    
    void TRANSLATION_API SharingContractPresent(_In_ Resource* pResource);
    void TRANSLATION_API Present(
        _In_reads_(numSrcSurfaces) PresentSurface const* pSrcSurfaces,
        UINT numSrcSurfaces,
        _In_opt_ Resource* pDest,
        UINT flipInterval,
        UINT vidPnSourceId,
        _In_ D3DKMT_PRESENT* pKMTPresent,
        bool bDoNotSequence,
        std::function<HRESULT(PresentCBArgs&)> pfnPresentCb);
    HRESULT TRANSLATION_API CloseAndSubmitGraphicsCommandListForPresent(
        BOOL commandsAdded,
        _In_reads_(numSrcSurfaces) const PresentSurface* pSrcSurfaces, 
        UINT numSrcSurfaces,
        _In_opt_ Resource* pDest,
        _In_ D3DKMT_PRESENT* pKMTPresent);

public:
    void TRANSLATION_API GetSharedGDIHandle(_In_ Resource *pResource, _Out_ HANDLE *pHandle);
    void TRANSLATION_API CreateSharedNTHandle(_In_ Resource *pResource, _Out_ HANDLE *pHandle, _In_opt_ SECURITY_ATTRIBUTES *pSA = nullptr);

    bool ResourceAllocationFallback(ResourceAllocationContext threadingContext);

    template <typename TFunc>
    auto TryAllocateResourceWithFallback(TFunc&& allocateFunc, ResourceAllocationContext threadingContext)
    {
        while (true)
        {
            try
            {
                return allocateFunc();
            }
            catch( _com_error& hrEx )
            {
                if (hrEx.Error() != E_OUTOFMEMORY ||
                    !ResourceAllocationFallback(threadingContext))
                {
                    throw;
                }
            }
        }
    }

public: // Type 
    // Note: all interfaces in these structs have weak refs
    // Bindings are remembered separate from immediate context to compute diff for state transitions
    struct SStageState
    {
        SStageState() noexcept(false) = default;
        void ClearState(EShaderStage stage) noexcept;

        // Shader-declared bindings do not set pipeline dirty bits at bind time, only slot dirty bits
        // These slot dirty bits are only interesting if they are below the maximum shader-declared slot,
        // as determined during pre-draw/dispatch based on the bound shaders
        CViewBoundState<TSRV, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> m_SRVs;
        CConstantBufferBoundState m_CBs;
        CSamplerBoundState m_Samplers;

        // Slots for re-asserting state on a new command list
        D3D12_GPU_DESCRIPTOR_HANDLE m_SRVTableBase{ 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_CBTableBase{ 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_SamplerTableBase{ 0 };

        UINT m_uConstantBufferOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT] = {};
        UINT m_uConstantBufferCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT] = {};
    };

    struct SState
    {
        SState() noexcept(false) = default;
        void ClearState() noexcept;

        PipelineState* m_pPSO = nullptr;
        RootSignature* m_pLastGraphicsRootSig = nullptr, *m_pLastComputeRootSig = nullptr;

        Query* m_pPredicate = nullptr;
        
        CViewBoundState<UAV, D3D11_1_UAV_SLOT_COUNT> m_UAVs, m_CSUAVs;

        CSimpleBoundState<TRTV, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> m_RTVs;
        CSimpleBoundState<TDSV, 1> m_DSVs;
        CSimpleBoundState<Resource, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, VBBinder> m_VBs;
        CSimpleBoundState<Resource, D3D11_SO_STREAM_COUNT, SOBinder> m_SO;
        CSimpleBoundState<Resource, 1, IBBinder> m_IB;
        UINT m_LastVBCount = 0;

        // Slots for re-asserting state on a new command list
        D3D12_GPU_DESCRIPTOR_HANDLE m_UAVTableBase{ 0 }, m_CSUAVTableBase{ 0 };

        SStageState& GetStageState(EShaderStage) noexcept;
        SStageState m_PS, m_VS, m_GS, m_HS, m_DS, m_CS;
    };

    void PreRender(COMMAND_LIST_TYPE type) noexcept; // draw, dispatch, clear, copy, etc
    void PostRender(COMMAND_LIST_TYPE type, UINT64 ReassertBitsToAdd = 0);

    D3D12_BOX GetBoxFromResource(Resource *pSrc, UINT SrcSubresource);
    D3D12_BOX GetSubresourceBoxFromBox(Resource* pSrc, UINT RequestedSubresource, UINT BaseSubresource, D3D12_BOX const& SrcBox);

    class BltResolveManager
    {
        D3D12TranslationLayer::ImmediateContext& m_ImmCtx;
        std::map<HWND, unique_comptr<Resource>> m_Temps;
    public:
        BltResolveManager(D3D12TranslationLayer::ImmediateContext& ImmCtx);
        Resource* GetBltResolveTempForWindow(HWND hwnd, Resource& presentingResource);
    } m_BltResolveManager;

private: // methods
    void PreDraw() noexcept(false);
    void PreDispatch() noexcept(false);
    
    // The app should inform the translation layer when a frame has been finished
    // to hint when trimming work should start
    //
    // The translation layer makes guesses at frame ends (i.e. when flush is called) 
    // but isn't aware when a present is done.
    void TRANSLATION_API PostSubmitNotification();

    void PostDraw();
    void PostDispatch();

    void SameResourceCopy(Resource *pSrc, UINT SrcSubresource, Resource *pDst, UINT DstSubresource, UINT dstX, UINT dstY, UINT dstZ, const D3D12_BOX *pSrcBox);

public:
    void PostCopy(Resource *pSrc, UINT startSubresource, Resource *pDest, UINT dstSubresource, UINT totalNumSubresources);
    void PostUpload();

    void CopyDataToBuffer(
        ID3D12Resource* pResource,
        UINT Offset,
        const void* pData,
        UINT Size
        ) noexcept(false);

    bool HasCommands(COMMAND_LIST_TYPE type) noexcept;
    UINT GetCommandListTypeMaskForQuery(EQueryType query) noexcept;

    void PrepForCommandQueueSync(UINT commandListTypeMask);

    RootSignature* CreateOrRetrieveRootSignature(RootSignatureDesc const& desc) noexcept(false);

private:
    bool Shutdown() noexcept;

    template <bool bDispatch> UINT CalculateViewSlotsForBindings() noexcept;
    template <bool bDispatch> UINT CalculateSamplerSlotsForBindings() noexcept;

    // Mark used in command list, copy to descriptor heap, and bind table
    template<EShaderStage eShader>
    void DirtyShaderResourcesHelper(UINT& HeapSlot) noexcept;
    template<EShaderStage eShader>
    void DirtyConstantBuffersHelper(UINT& HeapSlot) noexcept;
    template<EShaderStage eShader>
    void DirtySamplersHelper(UINT& HeapSlot) noexcept;

    // Mark used in command list and bind table (descriptors already in heap)
    template<EShaderStage eShader>
    void ApplyShaderResourcesHelper() noexcept;
    template<EShaderStage eShader>
    void ApplyConstantBuffersHelper() noexcept;
    template<EShaderStage eShader>
    void ApplySamplersHelper() noexcept;

    void SetScissorRectsHelper() noexcept;
    void RefreshNonHeapBindings(UINT64 DirtyBits) noexcept;
    ID3D12PipelineState* PrepareGenerateMipsObjects(DXGI_FORMAT Format, D3D12_RESOURCE_DIMENSION Dimension) noexcept(false);
    void EnsureInternalUAVRootSig() noexcept(false);
    void EnsureDrawAutoResources() noexcept(false);
    void EnsureQueryResources() noexcept(false);
    void EnsureExecuteIndirectResources() noexcept(false);

    static const UINT NUM_UAV_ROOT_SIG_CONSTANTS = 2;
    void FormatBuffer(ID3D12Resource* pBuffer, ID3D12PipelineState* pPSO, UINT FirstElement, UINT NumElements, const UINT Constants[NUM_UAV_ROOT_SIG_CONSTANTS] ) noexcept(false);

    // Helper for views
    void TransitionResourceForView(ViewBase* pView, D3D12_RESOURCE_STATES desiredState) noexcept;
    template<typename TIface> void ClearViewWithNoRenderTarget(View<TIface>* pView, CONST FLOAT[4], UINT NumRects, const D3D12_RECT *pRects);

    UINT GetCurrentCommandListTypeMask() noexcept;

    void InsertUAVBarriersIfNeeded(CViewBoundState<UAV, D3D11_1_UAV_SLOT_COUNT>& UAVBindings, UINT NumUAVs) noexcept;

public: // Methods
    UINT GetNodeMask() const noexcept
    {
        return 1 << m_nodeIndex;
    }

    UINT GetNodeIndex() const noexcept
    {
        return m_nodeIndex;
    }

    D3D12_HEAP_PROPERTIES GetHeapProperties(D3D12_HEAP_TYPE Type) const noexcept
    {
        if (ComputeOnly())
        {
            return CD3DX12_HEAP_PROPERTIES(Type, GetNodeMask(), GetNodeMask());
        }
        else
        {
            return m_pDevice12->GetCustomHeapProperties(GetNodeMask(), Type);
        }
    }

    const D3D12_FEATURE_DATA_D3D12_OPTIONS& GetCaps() { return m_caps; }
    bool ComputeOnly() const {return !!(FeatureLevel() == D3D_FEATURE_LEVEL_1_0_CORE);}
public: // variables
    // D3D11 objects
    UINT m_uStencilRef;
    float m_BlendFactor[4];
    D3D12_PRIMITIVE_TOPOLOGY m_PrimitiveTopology;
    BOOL m_PredicateValue;
    UINT  m_auVertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    UINT m_auVertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    DXGI_FORMAT m_IndexBufferFormat;
    UINT m_uIndexBufferOffset;
    UINT m_uNumScissors;
    D3D12_RECT m_aScissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT m_uNumViewports;
    D3D12_VIEWPORT m_aViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    BOOL m_ScissorRectEnable;

    std::unordered_map<RootSignatureDesc, std::unique_ptr<RootSignature>> m_RootSignatures;

    std::unique_ptr<CThreadPool> m_spPSOCompilationThreadPool;

    // "Online" descriptor heaps
    struct OnlineDescriptorHeap
    {
        unique_comptr<ID3D12DescriptorHeap> m_pDescriptorHeap;
        decltype(D3D12_GPU_DESCRIPTOR_HANDLE::ptr) m_DescriptorHeapBase;
        decltype(D3D12_CPU_DESCRIPTOR_HANDLE::ptr) m_DescriptorHeapBaseCPU;

        D3D12_DESCRIPTOR_HEAP_DESC m_Desc;
        UINT m_DescriptorSize;
        UINT m_BitsToSetOnNewHeap = 0;
        UINT m_MaxHeapSize;

        CFencedRingBuffer m_DescriptorRingBuffer;

        CFencePool< unique_comptr<ID3D12DescriptorHeap> > m_HeapPool;

        inline D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(UINT slot) { 
            assert(slot < m_Desc.NumDescriptors);
            return { m_DescriptorHeapBaseCPU + slot * m_DescriptorSize }; 
        }
        inline D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(UINT slot) { 
            assert(slot < m_Desc.NumDescriptors);
            return { m_DescriptorHeapBase + slot * m_DescriptorSize }; 
        }
    } m_ViewHeap, m_SamplerHeap;

    void RollOverHeap(OnlineDescriptorHeap& Heap) noexcept(false);
    UINT ReserveSlotsForBindings(OnlineDescriptorHeap& Heap, UINT (ImmediateContext::*pfnCalcRequiredSlots)()) noexcept(false);
    UINT ReserveSlots(OnlineDescriptorHeap& Heap, UINT NumSlots) noexcept(false);

    D3D12_CPU_DESCRIPTOR_HANDLE m_NullSRVs[(UINT)RESOURCE_DIMENSION::TEXTURECUBEARRAY+1];
    D3D12_CPU_DESCRIPTOR_HANDLE m_NullUAVs[(UINT)RESOURCE_DIMENSION::TEXTURECUBEARRAY+1];
    D3D12_CPU_DESCRIPTOR_HANDLE m_NullRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_NullSampler;
    TDeclVector m_UAVDeclScratch;

    // Offline descriptor heaps
    CDescriptorHeapManager m_SRVAllocator;
    CDescriptorHeapManager m_UAVAllocator;
    CDescriptorHeapManager m_RTVAllocator;
    CDescriptorHeapManager m_DSVAllocator;
    CDescriptorHeapManager m_SamplerAllocator;

    ResourceCache m_ResourceCache;
    std::vector<D3D12_RECT> m_RectCache;

    // UAV barriers are not managed by the state manager.
    // The state manager deals with changes in state, where UAV barriers need to be inserted
    // in steady-state scenarios.
    std::vector<D3D12_RESOURCE_BARRIER> m_vUAVBarriers;

    // Objects for GenerateMips
    typedef std::tuple<DXGI_FORMAT, D3D12_RESOURCE_DIMENSION> MipGenKey;
    std::map<MipGenKey, unique_comptr<ID3D12PipelineState>> m_pGenerateMipsPSOMap;
    InternalRootSignature m_GenerateMipsRootSig;
    enum GenerateMipsRootSignatureSlots
    {
        eSRV = 0,
        eRootConstants,
        eSampler,
    };

    static const UINT NUM_FILTER_TYPES = 2;
    D3D12_CPU_DESCRIPTOR_HANDLE m_GenerateMipsSamplers[NUM_FILTER_TYPES];

    BlitHelper m_BlitHelper{ this };

    template <typename TIface> CDescriptorHeapManager& GetViewAllocator();
    template<> CDescriptorHeapManager& GetViewAllocator<ShaderResourceViewType>() { return m_SRVAllocator; }
    template<> CDescriptorHeapManager& GetViewAllocator<UnorderedAccessViewType>() { return m_UAVAllocator; }
    template<> CDescriptorHeapManager& GetViewAllocator<RenderTargetViewType>() { return m_RTVAllocator; }
    template<> CDescriptorHeapManager& GetViewAllocator<DepthStencilViewType>() { return m_DSVAllocator; }

    InternalRootSignature m_InternalUAVRootSig;
    unique_comptr<ID3D12PipelineState> m_pDrawAutoPSO;
    unique_comptr<ID3D12PipelineState> m_pFormatQueryPSO;
    unique_comptr<ID3D12PipelineState> m_pAccumulateQueryPSO;

    unique_comptr<ID3D12CommandSignature> m_pDrawInstancedCommandSignature;
    unique_comptr<ID3D12CommandSignature> m_pDrawIndexedInstancedCommandSignature;
    unique_comptr<ID3D12CommandSignature> m_pDispatchCommandSignature;

    LIST_ENTRY m_ActiveQueryList;

    D3D_FEATURE_LEVEL FeatureLevel() const { return m_FeatureLevel; }

    static DXGI_FORMAT GetParentForFormat(DXGI_FORMAT format);

    bool UseRoundTripPSOs()
    {
        return m_CreationArgs.UseRoundTripPSOs;
    }

    TranslationLayerCallbacks const& GetUpperlayerCallbacks() { return m_callbacks; }

    ResidencyManager &GetResidencyManager() { return m_residencyManager; }
    ResourceStateManager& GetResourceStateManager() { return m_ResourceStateManager; }

    MaxFrameLatencyHelper m_MaxFrameLatencyHelper;
private: // variables
    ResourceStateManager m_ResourceStateManager;
#if TRANSLATION_LAYER_DBG
    UINT64 m_DebugFlags;
#endif

    unique_comptr<Resource> m_pStagingTexture;
    unique_comptr<Resource> m_pStagingBuffer;

private: // Dynamic/staging resource pools
    const UINT64 m_BufferPoolTrimThreshold = 100;
    TDynamicBufferPool m_UploadBufferPool;
    TDynamicBufferPool m_ReadbackBufferPool;
    TDynamicBufferPool m_DecoderBufferPool;
    TDynamicBufferPool& GetBufferPool(AllocatorHeapType HeapType)
    {
        switch (HeapType)
        {
        case AllocatorHeapType::Upload:
            return m_UploadBufferPool;
        case AllocatorHeapType::Readback:
            return m_ReadbackBufferPool;
        case AllocatorHeapType::Decoder:
            return m_DecoderBufferPool;
        default:
            assert(false);
        }
        return m_UploadBufferPool;
    }

    // This is the maximum amount of memory the buddy allocator can use. Picking an abritrarily high
    // cap that allows this to pass tests that can potentially spend the whole GPU's memory on
    // suballocated heaps
    static constexpr UINT64 cBuddyMaxBlockSize = 32ll * 1024ll * 1024ll * 1024ll;
    static bool ResourceNeedsOwnAllocation(UINT64 size, bool cannotBeOffset)
    {
        return size > cBuddyAllocatorThreshold || cannotBeOffset;
    }

    // These suballocate out of larger heaps. This should not 
    // be used for resources that require transitions since transitions
    // can only be done on the entire heap, not just the suballocated range
    ConditionalHeapAllocator m_UploadHeapSuballocator;
    ConditionalHeapAllocator m_ReadbackHeapSuballocator;
    ConditionalHeapAllocator m_DecoderHeapSuballocator;
    ConditionalHeapAllocator& GetAllocator(AllocatorHeapType HeapType)
    {
        switch (HeapType)
        {
        case AllocatorHeapType::Upload:
            return m_UploadHeapSuballocator;
        case AllocatorHeapType::Readback:
            return m_ReadbackHeapSuballocator;
        case AllocatorHeapType::Decoder:
            return m_DecoderHeapSuballocator;
        default:
            assert(false);
        }
        return m_UploadHeapSuballocator;
    }

    COptLockedContainer<RenameResourceSet> m_RenamesInFlight;

private: // State tracking
    // Dirty states are marked during sets and converted to command list operations at draw time, to avoid multiple costly conversions due to 11/12 API differences
    UINT64 m_DirtyStates;

    // Set to be all states during Flush, bits are cleared as individual sets come in, and all remaining bits are re-asserted on new command lists at draw time
    UINT64 m_StatesToReassert;

    SState m_CurrentState;

    UINT m_nodeIndex;
    D3D12_FEATURE_DATA_D3D12_OPTIONS m_caps;
    const TranslationLayerCallbacks m_callbacks;

private:
    static inline bool IsSingleCommandListType(UINT commandListTypeMask)
    {
        commandListTypeMask &= ~COMMAND_LIST_TYPE_UNKNOWN_MASK;     // ignore UNKNOWN type
        return commandListTypeMask & (commandListTypeMask - 1) ? false : true;
    }

    void DiscardViewImpl(COMMAND_LIST_TYPE commandListType, ViewBase* pView, const D3D12_RECT*, UINT, bool allSubresourcesSame);
    void DiscardResourceImpl(COMMAND_LIST_TYPE commandListType, Resource* pResource, const D3D12_RECT* pRects, UINT NumRects, bool allSubresourcesSame);
    void UpdateTileMappingsImpl(COMMAND_LIST_TYPE commandListType, Resource* pResource, UINT NumTiledResourceRegions, _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords, 
                            _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes, Resource* pTilePool, UINT NumRanges, _In_reads_opt_(NumRanges) const TILE_RANGE_FLAG* pRangeFlags, 
                            _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets, _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts, TILE_MAPPING_FLAG Flags, bool NeedToSubmit);
    void CopyTileMappingsImpl(COMMAND_LIST_TYPE commandListType, Resource* pDstTiledResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstStartCoords, Resource* pSrcTiledResource,
                          _In_ const  D3D12_TILED_RESOURCE_COORDINATE* pSrcStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, TILE_MAPPING_FLAG Flags);
    void TiledResourceBarrierImpl(COMMAND_LIST_TYPE commandListType, Resource* pBefore, Resource* pAfter);
    COMMAND_LIST_TYPE GetFallbackCommandListType(UINT commandListTypeMask);

    // Device wide scratch space allocation for use in synchronous ops.
    // Only grows.  Free with device.
    struct
    {
        BYTE* GetBuffer(UINT minSize)
        {
            if (minSize > m_Size)
            {
                m_spScratchBuffer = std::make_unique<BYTE[]>(minSize);
                m_Size = minSize;
            }

            return m_spScratchBuffer.get();
        }

    private:
        std::unique_ptr<BYTE[]> m_spScratchBuffer;
        UINT m_Size = 0;

    } m_SyncronousOpScrachSpace;

    const bool m_bUseRingBufferDescriptorHeaps;
};

DEFINE_ENUM_FLAG_OPERATORS(ImmediateContext::UpdateSubresourcesFlags);

struct SafeRenameResourceCookie
{
    SafeRenameResourceCookie(Resource* c = nullptr) : m_c(c) { }
    Resource* Detach() { auto c = m_c; m_c = nullptr; return c; }
    Resource* Get() { return m_c; }
    void Delete()
    {
        if (m_c)
        {
            m_c->m_pParent->DeleteRenameCookie(m_c);
            m_c = nullptr;
        }
    }
    void Reset(Resource* c) { Delete(); m_c = c; }
    ~SafeRenameResourceCookie() { Delete(); }
    Resource* m_c = nullptr;
};

} // namespace D3D12TranslationLayer
    
