// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class CommandListManager;

// These are defined in the private d3d12 header
#define UNKNOWN_RESOURCE_STATE (D3D12_RESOURCE_STATES)0x8000u
#define RESOURCE_STATE_VALID_BITS 0x2f3fff
#define RESOURCE_STATE_VALID_INTERNAL_BITS 0x2fffff
constexpr D3D12_RESOURCE_STATES RESOURCE_STATE_ALL_WRITE_BITS =
    D3D12_RESOURCE_STATE_RENDER_TARGET          |
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS       |
    D3D12_RESOURCE_STATE_DEPTH_WRITE            |
    D3D12_RESOURCE_STATE_STREAM_OUT             |
    D3D12_RESOURCE_STATE_COPY_DEST              |
    D3D12_RESOURCE_STATE_RESOLVE_DEST           |
    D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE     |
    D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE;

    enum class SubresourceTransitionFlags
    {
        None = 0,
        TransitionPreDraw = 1,
        NoBindingTransitions = 2,
        StateMatchExact = 4,
        ForceExclusiveState = 8,
        NotUsedInCommandListIfNoStateChange = 0x10,
    };
    DEFINE_ENUM_FLAG_OPERATORS(SubresourceTransitionFlags);

    inline bool IsD3D12WriteState(UINT State, SubresourceTransitionFlags Flags)
    {
        return (State & RESOURCE_STATE_ALL_WRITE_BITS) != 0 ||
            (Flags & SubresourceTransitionFlags::ForceExclusiveState) != SubresourceTransitionFlags::None;
    }

    //==================================================================================================================================
    // CDesiredResourceState
    // Stores the current desired state of either an entire resource, or each subresource.
    //==================================================================================================================================
    class CDesiredResourceState
    {
    public:
        struct SubresourceInfo
        {
            D3D12_RESOURCE_STATES State = UNKNOWN_RESOURCE_STATE;
            COMMAND_LIST_TYPE CommandListType = COMMAND_LIST_TYPE::UNKNOWN;
            SubresourceTransitionFlags Flags = SubresourceTransitionFlags::None;
        };

    private:
        bool m_bAllSubresourcesSame = true;

        PreallocatedInlineArray<SubresourceInfo, 1> m_spSubresourceInfo;

    public:
        static size_t CalcPreallocationSize(UINT SubresourceCount) { return sizeof(SubresourceInfo) * (SubresourceCount - 1); }
        CDesiredResourceState(UINT SubresourceCount, void*& pPreallocatedMemory) noexcept
            : m_spSubresourceInfo(SubresourceCount, pPreallocatedMemory) // throw( bad_alloc )
        {
        }

        bool AreAllSubresourcesSame() const noexcept { return m_bAllSubresourcesSame; }

        SubresourceInfo const& GetSubresourceInfo(UINT SubresourceIndex) const noexcept;
        void SetResourceState(SubresourceInfo const& Info) noexcept;
        void SetSubresourceState(UINT SubresourceIndex, SubresourceInfo const& Info) noexcept;

        void Reset() noexcept;
    };

    //==================================================================================================================================
    // CCurrentResourceState
    // Stores the current state of either an entire resource, or each subresource.
    // Current state can either be shared read across multiple queues, or exclusive on a single queue.
    //==================================================================================================================================
    class CCurrentResourceState
    {
    public:
        static constexpr unsigned NumCommandListTypes = static_cast<UINT>(COMMAND_LIST_TYPE::MAX_VALID);
        struct ExclusiveState
        {
            UINT64 FenceValue = 0;
            D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
            COMMAND_LIST_TYPE CommandListType = COMMAND_LIST_TYPE::UNKNOWN;

            // There are cases where we want to synchronize against last write, instead
            // of last access. Therefore the exclusive state of a (sub)resource is not
            // overwritten when transitioning to shared state, simply marked as stale.
            // So Map(READ) always synchronizes against the most recent exclusive state,
            // while Map(WRITE) always synchronizes against the most recent state, whether
            // it's exclusive or shared.
            bool IsMostRecentlyExclusiveState = true;
        };
        struct SharedState
        {
            UINT64 FenceValues[NumCommandListTypes] = {};
            D3D12_RESOURCE_STATES State[NumCommandListTypes] = {};
        };

    private:
        const bool m_bSimultaneousAccess;
        bool m_bAllSubresourcesSame = true;

        // Note: As a (minor) memory optimization, using a contiguous block of memory for exclusive + shared state.
        // The memory is owned by the exclusive state pointer. The shared state pointer is non-owning and possibly null.
        PreallocatedInlineArray<ExclusiveState, 1> m_spExclusiveState;
        PreallocatedInlineArray<SharedState, 1> m_pSharedState;

        void ConvertToSubresourceTracking() noexcept;

    public:
        static size_t CalcPreallocationSize(UINT SubresourceCount, bool bSimultaneousAccess)
        {
            return (sizeof(ExclusiveState) + (bSimultaneousAccess ? sizeof(SharedState) : 0u)) * (SubresourceCount - 1);
        }
        CCurrentResourceState(UINT SubresourceCount, bool bSimultaneousAccess, void*& pPreallocatedMemory) noexcept;

        bool SupportsSimultaneousAccess() const noexcept { return m_bSimultaneousAccess; }
        bool AreAllSubresourcesSame() const noexcept { return m_bAllSubresourcesSame; }

        bool IsExclusiveState(UINT SubresourceIndex) const noexcept;

        void SetExclusiveResourceState(ExclusiveState const& State) noexcept;
        void SetSharedResourceState(COMMAND_LIST_TYPE Type, UINT64 FenceValue, D3D12_RESOURCE_STATES State) noexcept;
        void SetExclusiveSubresourceState(UINT SubresourceIndex, ExclusiveState const& State) noexcept;
        void SetSharedSubresourceState(UINT SubresourceIndex, COMMAND_LIST_TYPE Type, UINT64 FenceValue, D3D12_RESOURCE_STATES State) noexcept;
        ExclusiveState const& GetExclusiveSubresourceState(UINT SubresourceIndex) const noexcept;
        SharedState const& GetSharedSubresourceState(UINT SubresourceIndex) const noexcept;

        UINT GetCommandListTypeMask() const noexcept;
        UINT GetCommandListTypeMask(CViewSubresourceSubset const& Subresources) const noexcept;
        UINT GetCommandListTypeMask(UINT Subresource) const noexcept;

        void Reset() noexcept;
    };

    //==================================================================================================================================
    // DeferredWait
    //==================================================================================================================================
    struct DeferredWait
    {
        std::shared_ptr<Fence> fence;
        UINT64 value;
    };
    
    //==================================================================================================================================
    // TransitionableResourceBase
    // A base class that transitionable resources should inherit from.
    //==================================================================================================================================
    struct TransitionableResourceBase
    {
        LIST_ENTRY m_TransitionListEntry;
        CDesiredResourceState m_DesiredState;
        const bool m_bTriggersSwapchainDeferredWaits;
        std::vector<DeferredWait> m_ResourceDeferredWaits;

        static size_t CalcPreallocationSize(UINT NumSubresources) { return CDesiredResourceState::CalcPreallocationSize(NumSubresources); }
        TransitionableResourceBase(UINT NumSubresources, bool bTriggersDeferredWaits, void*& pPreallocatedMemory) noexcept
            : m_DesiredState(NumSubresources, pPreallocatedMemory)
            , m_bTriggersSwapchainDeferredWaits(bTriggersDeferredWaits)
        {
            D3D12TranslationLayer::InitializeListHead(&m_TransitionListEntry);
        }
        ~TransitionableResourceBase() noexcept
        {
            if (IsTransitionPending())
            {
                D3D12TranslationLayer::RemoveEntryList(&m_TransitionListEntry);
            }
        }
        bool IsTransitionPending() const noexcept { return !D3D12TranslationLayer::IsListEmpty(&m_TransitionListEntry); }

        void AddDeferredWaits(const std::vector<DeferredWait>& DeferredWaits) noexcept(false)
        {
            m_ResourceDeferredWaits.insert(m_ResourceDeferredWaits.end(), DeferredWaits.begin(), DeferredWaits.end()); // throw( bad_alloc )
        }
    };

    //==================================================================================================================================
    // ResourceStateManagerBase
    // The main business logic for handling resource transitions, including multi-queue sync and shared/exclusive state changes.
    //
    // Requesting a resource to transition simply updates destination state, and ensures it's in a list to be processed later.
    //
    // When processing ApplyAllResourceTransitions, we build up sets of vectors.
    // There's a source one for each command list type, and a single one for the dest because we are applying
    // the resource transitions for a single operation.
    // There's also a vector for "tentative" barriers, which are merged into the destination vector if
    // no flushing occurs as a result of submitting the final barrier operation.
    // 99% of the time, there will only be the source being populated, but sometimes there will be a destination as well.
    // If the source and dest of a transition require different types, we put a (source->COMMON) in the approriate source vector,
    // and a (COMMON->dest) in the destination vector.
    //
    // Once all resources are processed, we:
    // 1. Submit all source barriers, except ones belonging to the destination queue.
    // 2. Flush all source command lists, except ones belonging to the destination queue.
    // 3. Determine if the destination queue is going to be flushed.
    //    If so: Submit source barriers on that command list first, then flush it.
    //    If not: Accumulate source, dest, and tentative barriers so they can be sent to D3D12 in a single API call.
    // 4. Insert waits on the destination queue - deferred waits, and waits for work on other queues.
    // 5. Insert destination barriers.
    //
    // Only once all of this has been done do we update the "current" state of resources,
    // because this is the only way that we know whether or not the destination queue has been flushed,
    // and therefore, we can get the correct fence values to store in the subresources.
    //==================================================================================================================================
    class ResourceStateManagerBase
    {
    protected:

        LIST_ENTRY m_TransitionListHead;
        std::vector<DeferredWait> m_SwapchainDeferredWaits;
        std::vector<DeferredWait> m_ResourceDeferredWaits;

        // State that is reset during the preamble, accumulated during resource traversal,
        // and applied during the submission phase.
        enum class PostApplyExclusiveState
        {
            Exclusive, Shared, SharedIfFlushed
        };
        struct PostApplyUpdate
        {
            TransitionableResourceBase& AffectedResource;
            CCurrentResourceState& CurrentState;
            UINT SubresourceIndex;
            D3D12_RESOURCE_STATES NewState;
            PostApplyExclusiveState ExclusiveState;
            bool WasTransitioningToDestinationType;
        };
        std::vector<D3D12_RESOURCE_BARRIER> m_vSrcResourceBarriers[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        std::vector<D3D12_RESOURCE_BARRIER> m_vDstResourceBarriers;
        std::vector<D3D12_RESOURCE_BARRIER> m_vTentativeResourceBarriers;
        std::vector<PostApplyUpdate> m_vPostApplyUpdates;
        COMMAND_LIST_TYPE m_DestinationCommandListType;
        bool m_bApplySwapchainDeferredWaits;
        bool m_bFlushQueues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT64 m_QueueFenceValuesToWaitOn[(UINT)COMMAND_LIST_TYPE::MAX_VALID];

        UINT64 m_InsertedQueueSync[(UINT)COMMAND_LIST_TYPE::MAX_VALID][(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};

        ResourceStateManagerBase() noexcept(false);

        ~ResourceStateManagerBase() noexcept
        {
            // All resources should be gone by this point, and each resource ensures it is no longer in this list.
            assert(D3D12TranslationLayer::IsListEmpty(&m_TransitionListHead));
        }

        // These methods set the destination state of the resource/subresources and ensure it's in the transition list.
        void TransitionResource(TransitionableResourceBase& Resource,
                                CDesiredResourceState::SubresourceInfo const& State) noexcept;
        void TransitionSubresources(TransitionableResourceBase& Resource,
                                    CViewSubresourceSubset const& Subresources,
                                    CDesiredResourceState::SubresourceInfo const& State) noexcept;
        void TransitionSubresource(TransitionableResourceBase& Resource,
                                   UINT SubresourceIndex,
                                   CDesiredResourceState::SubresourceInfo const& State) noexcept;

        // Deferred waits are inserted when a transition is processing that puts applicable resources
        // into a write state. The command list is flushed, and these waits are inserted before the barriers.
        void AddDeferredWait(std::shared_ptr<Fence> const& spFence, UINT64 Value) noexcept(false);

        // Clear out any state from previous iterations.
        void ApplyResourceTransitionsPreamble() noexcept;

        // What to do with the resource, in the context of the transition list, after processing it.
        enum class TransitionResult
        {
            // There are no more pending transitions that may be processed at a later time (i.e. draw time),
            // so remove it from the pending transition list.
            Remove,
            // There are more transitions to be done, so keep it in the list.
            Keep
        };

        // For every entry in the transition list, call a routine.
        // This routine must return a TransitionResult which indicates what to do with the list.
        template <typename TFunc>
        void ForEachTransitioningResource(TFunc&& func)
            noexcept(noexcept(func(std::declval<TransitionableResourceBase&>())))
        {
            for (LIST_ENTRY *pListEntry = m_TransitionListHead.Flink; pListEntry != &m_TransitionListHead;)
            {
                TransitionableResourceBase* pResource = CONTAINING_RECORD(pListEntry, TransitionableResourceBase, m_TransitionListEntry);
                TransitionResult result = func(*pResource);

                auto pNextListEntry = pListEntry->Flink;
                if (result == TransitionResult::Remove)
                {
                    D3D12TranslationLayer::RemoveEntryList(pListEntry);
                    D3D12TranslationLayer::InitializeListHead(pListEntry);
                }
                pListEntry = pNextListEntry;
            }
        }

        // Updates vectors with the operations that should be applied to the requested resource.
        // May update the destination state of the resource.
        TransitionResult ProcessTransitioningResource(ID3D12Resource* pTransitioningResource,
                                                      TransitionableResourceBase& TransitionableResource,
                                                      CCurrentResourceState& CurrentState,
                                                      CResourceBindings& BindingState,
                                                      UINT NumTotalSubresources,
                                                      _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
                                                      bool bIsPreDraw) noexcept(false);

        // This method is templated so that it can have the same code for two implementations without copy/paste.
        // It is not intended to be completely exstensible. One implementation is leveraged by the translation layer
        // itself using lambdas that can be inlined. The other uses std::functions which cannot and is intended for tests.
        template <
            typename TSubmitBarriersImpl,
            typename TSubmitCmdListImpl,
            typename THasCommandsImpl,
            typename TGetCurrentFenceImpl,
            typename TInsertDeferredWaitsImpl,
            typename TInsertQueueWaitImpl
        > void SubmitResourceTransitionsImpl(TSubmitBarriersImpl&&,
                                             TSubmitCmdListImpl&&,
                                             THasCommandsImpl&&,
                                             TGetCurrentFenceImpl&&,
                                             TInsertDeferredWaitsImpl&&,
                                             TInsertQueueWaitImpl&&);

        // Call the D3D12 APIs to perform the resource barriers, command list submission, and command queue sync
        // that was determined by previous calls to ProcessTransitioningResource.
        void SubmitResourceTransitions(_In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) CommandListManager** ppManagers) noexcept(false);
        
        // Call the callbacks provided to allow tests to inspect what D3D12 APIs would've been called
        // as part of finalizing a set of barrier operations.
        void SimulateSubmitResourceTransitions(std::function<void(std::vector<D3D12_RESOURCE_BARRIER>&, COMMAND_LIST_TYPE)> SubmitBarriers,
                                               std::function<void(COMMAND_LIST_TYPE)> SubmitCmdList,
                                               std::function<bool(COMMAND_LIST_TYPE)> HasCommands,
                                               std::function<UINT64(COMMAND_LIST_TYPE)> GetCurrentFenceImpl,
                                               std::function<void(COMMAND_LIST_TYPE)> InsertDeferredWaits,
                                               std::function<void(UINT64, COMMAND_LIST_TYPE Src, COMMAND_LIST_TYPE Dst)> InsertQueueWait);

        // Update the current state of resources now that all barriers and sync have been done.
        // Callback provided to allow this information to be used by concrete implementation as well.
        template <typename TFunc>
        void PostSubmitUpdateState(TFunc&& callback, _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* PreviousFenceValues, UINT64 NewFenceValue)
        {
            for (auto& update : m_vPostApplyUpdates)
            {
                COMMAND_LIST_TYPE UpdateCmdListType = m_DestinationCommandListType;
                UINT64 UpdateFenceValue = NewFenceValue;
                bool Flushed = m_DestinationCommandListType != COMMAND_LIST_TYPE::UNKNOWN &&
                    NewFenceValue != PreviousFenceValues[(UINT)m_DestinationCommandListType];
                if (update.ExclusiveState == PostApplyExclusiveState::Exclusive ||
                    (update.ExclusiveState == PostApplyExclusiveState::SharedIfFlushed && !Flushed))
                {
                    CCurrentResourceState::ExclusiveState NewExclusiveState = {};
                    if (update.WasTransitioningToDestinationType)
                    {
                        NewExclusiveState.CommandListType = m_DestinationCommandListType;
                        NewExclusiveState.FenceValue = NewFenceValue;
                    }
                    else
                    {
                        auto& OldExclusiveState = update.CurrentState.GetExclusiveSubresourceState(update.SubresourceIndex);
                        UpdateCmdListType = NewExclusiveState.CommandListType = OldExclusiveState.CommandListType;
                        UpdateFenceValue = NewExclusiveState.FenceValue = PreviousFenceValues[(UINT)OldExclusiveState.CommandListType];
                    }
                    NewExclusiveState.State = update.NewState;
                    if (update.SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
                    {
                        update.CurrentState.SetExclusiveResourceState(NewExclusiveState);
                    }
                    else
                    {
                        update.CurrentState.SetExclusiveSubresourceState(update.SubresourceIndex, NewExclusiveState);
                    }
                }
                else if (update.WasTransitioningToDestinationType)
                {
                    if (update.SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
                    {
                        update.CurrentState.SetSharedResourceState(m_DestinationCommandListType, NewFenceValue, update.NewState);
                    }
                    else
                    {
                        update.CurrentState.SetSharedSubresourceState(update.SubresourceIndex, m_DestinationCommandListType, NewFenceValue, update.NewState);
                    }
                }
                else
                {
                    continue;
                }
                callback(update, UpdateCmdListType, UpdateFenceValue);
            }
        }

    private:
        // Helpers
        static bool TransitionRequired(D3D12_RESOURCE_STATES CurrentState, D3D12_RESOURCE_STATES& DestinationState, SubresourceTransitionFlags Flags) noexcept;
        void AddCurrentStateUpdate(TransitionableResourceBase& Resource,
                                   CCurrentResourceState& CurrentState,
                                   UINT SubresourceIndex,
                                   D3D12_RESOURCE_STATES NewState,
                                   PostApplyExclusiveState ExclusiveState,
                                   bool IsGoingToDestinationType) noexcept(false);
        void ProcessTransitioningSubresourceExclusive(CCurrentResourceState& CurrentState,
                                                      UINT i,
                                                      COMMAND_LIST_TYPE curCmdListType,
                                                      _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
                                                      CDesiredResourceState::SubresourceInfo& SubresourceDestinationInfo,
                                                      D3D12_RESOURCE_STATES after,
                                                      TransitionableResourceBase& TransitionableResource,
                                                      D3D12_RESOURCE_BARRIER& TransitionDesc,
                                                      SubresourceTransitionFlags Flags) noexcept(false);
        void ProcessTransitioningSubresourceShared(CCurrentResourceState& CurrentState,
                                                   UINT i,
                                                   D3D12_RESOURCE_STATES after,
                                                   SubresourceTransitionFlags Flags,
                                                   _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
                                                   COMMAND_LIST_TYPE curCmdListType,
                                                   D3D12_RESOURCE_BARRIER& TransitionDesc,
                                                   TransitionableResourceBase& TransitionableResource) noexcept(false);
        void SubmitResourceBarriers(_In_reads_(Count) D3D12_RESOURCE_BARRIER const* pBarriers, UINT Count, _In_ CommandListManager* pManager) noexcept;
    };

    //==================================================================================================================================
    // ResourceStateManager
    // The implementation of state management tailored to the ImmediateContext and Resource classes.
    //==================================================================================================================================
    class ResourceStateManager : public ResourceStateManagerBase
    {
    private:
        ImmediateContext& m_ImmCtx;

    public:
        ResourceStateManager(ImmediateContext& ImmCtx)
            : m_ImmCtx(ImmCtx)
        {
        }

        // *** NOTE: DEFAULT DESTINATION IS GRAPHICS, NOT INFERRED FROM STATE BITS. ***

        // Transition the entire resource to a particular destination state on a particular command list.
        void TransitionResource(Resource* pResource,
                                D3D12_RESOURCE_STATES State,
                                COMMAND_LIST_TYPE DestinationCommandListType = COMMAND_LIST_TYPE::GRAPHICS,
                                SubresourceTransitionFlags Flags = SubresourceTransitionFlags::None) noexcept;
        // Transition a set of subresources to a particular destination state. Fast-path provided when subset covers entire resource.
        void TransitionSubresources(Resource* pResource,
                                    CViewSubresourceSubset const& Subresources,
                                    D3D12_RESOURCE_STATES State,
                                    COMMAND_LIST_TYPE DestinationCommandListType = COMMAND_LIST_TYPE::GRAPHICS,
                                    SubresourceTransitionFlags Flags = SubresourceTransitionFlags::None) noexcept;
        // Transition a single subresource to a particular destination state.
        void TransitionSubresource(Resource* pResource,
                                   UINT SubresourceIndex,
                                   D3D12_RESOURCE_STATES State,
                                   COMMAND_LIST_TYPE DestinationCommandListType = COMMAND_LIST_TYPE::GRAPHICS,
                                   SubresourceTransitionFlags Flags = SubresourceTransitionFlags::None) noexcept;
        // Update destination state of a resource to correspond to the resource's bind points.
        void TransitionResourceForBindings(Resource* pResource) noexcept;
        // Update destination state of specified subresources to correspond to the resource's bind points.
        void TransitionSubresourcesForBindings(Resource* pResource,
                                               CViewSubresourceSubset const& Subresources) noexcept;

        // Submit all barriers and queue sync.
        void ApplyAllResourceTransitions(bool bIsPreDraw = false) noexcept(false);

        using ResourceStateManagerBase::AddDeferredWait;
    };
};