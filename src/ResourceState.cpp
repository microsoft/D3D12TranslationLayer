// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{
    //----------------------------------------------------------------------------------------------------------------------------------
    auto CDesiredResourceState::GetSubresourceInfo(UINT SubresourceIndex) const noexcept -> SubresourceInfo const&
    {
        if (AreAllSubresourcesSame())
        {
            SubresourceIndex = 0;
        }
        return m_spSubresourceInfo[SubresourceIndex];
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CDesiredResourceState::SetResourceState(SubresourceInfo const & Info) noexcept
    {
        m_bAllSubresourcesSame = true;
        m_spSubresourceInfo[0] = Info;
    }
    
    //----------------------------------------------------------------------------------------------------------------------------------
    void CDesiredResourceState::SetSubresourceState(UINT SubresourceIndex, SubresourceInfo const & Info) noexcept
    {
        if (m_bAllSubresourcesSame && m_spSubresourceInfo.size() > 1)
        {
            static_assert(std::extent_v<decltype(m_spSubresourceInfo.m_InlineArray)> == 1, "Otherwise this fill doesn't work");
            std::fill(m_spSubresourceInfo.m_Extra.begin(), m_spSubresourceInfo.m_Extra.end(), m_spSubresourceInfo[0]);
            m_bAllSubresourcesSame = false;
        }
        if (m_spSubresourceInfo.size() == 1)
        {
            SubresourceIndex = 0;
        }
        m_spSubresourceInfo[SubresourceIndex] = Info;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CDesiredResourceState::Reset() noexcept
    {
        SetResourceState(SubresourceInfo{});
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::ConvertToSubresourceTracking() noexcept
    {
        if (m_bAllSubresourcesSame && m_spExclusiveState.size() > 1)
        {
            static_assert(std::extent_v<decltype(m_spExclusiveState.m_InlineArray)> == 1, "Otherwise this fill doesn't work");
            std::fill(m_spExclusiveState.m_Extra.begin(), m_spExclusiveState.m_Extra.end(), m_spExclusiveState[0]);
            if (!m_pSharedState.empty())
            {
                static_assert(std::extent_v<decltype(m_pSharedState.m_InlineArray)> == 1, "Otherwise this fill doesn't work");
                std::fill(m_pSharedState.m_Extra.begin(), m_pSharedState.m_Extra.end(), m_pSharedState[0]);
            }
            m_bAllSubresourcesSame = false;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    CCurrentResourceState::CCurrentResourceState(UINT SubresourceCount, bool bSimultaneousAccess, void*& pPreallocatedMemory) noexcept
        : m_bSimultaneousAccess(bSimultaneousAccess)
        , m_spExclusiveState(SubresourceCount, pPreallocatedMemory)
        , m_pSharedState(bSimultaneousAccess ? SubresourceCount : 0u, pPreallocatedMemory)
    {
        m_spExclusiveState[0] = ExclusiveState{};
        if (bSimultaneousAccess)
        {
            m_spExclusiveState[0].IsMostRecentlyExclusiveState = false;
            m_pSharedState[0] = SharedState{};
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool CCurrentResourceState::IsExclusiveState(UINT SubresourceIndex) const noexcept
    {
        if (!SupportsSimultaneousAccess())
        {
            return true;
        }
        if (AreAllSubresourcesSame())
        {
            SubresourceIndex = 0;
        }
        return m_spExclusiveState[SubresourceIndex].IsMostRecentlyExclusiveState;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::SetExclusiveResourceState(ExclusiveState const& State) noexcept
    {
        m_bAllSubresourcesSame = true;
        m_spExclusiveState[0] = State;
        if (!m_pSharedState.empty())
        {
            m_pSharedState[0] = SharedState{};
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::SetSharedResourceState(COMMAND_LIST_TYPE Type, UINT64 FenceValue, D3D12_RESOURCE_STATES State) noexcept
    {
        assert(!IsD3D12WriteState(State, SubresourceTransitionFlags::None));
        m_bAllSubresourcesSame = true;
        m_spExclusiveState[0].IsMostRecentlyExclusiveState = false;
        m_pSharedState[0].FenceValues[(UINT)Type] = FenceValue;
        m_pSharedState[0].State[(UINT)Type] = State;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::SetExclusiveSubresourceState(UINT SubresourceIndex, ExclusiveState const& State) noexcept
    {
        ConvertToSubresourceTracking();
        m_spExclusiveState[SubresourceIndex] = State;
        if (!m_pSharedState.empty())
        {
            m_pSharedState[SubresourceIndex] = SharedState{};
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::SetSharedSubresourceState(UINT SubresourceIndex, COMMAND_LIST_TYPE Type, UINT64 FenceValue, D3D12_RESOURCE_STATES State) noexcept
    {
        assert(!IsD3D12WriteState(State, SubresourceTransitionFlags::None));
        ConvertToSubresourceTracking();
        m_spExclusiveState[SubresourceIndex].IsMostRecentlyExclusiveState = false;
        m_pSharedState[SubresourceIndex].FenceValues[(UINT)Type] = FenceValue;
        m_pSharedState[SubresourceIndex].State[(UINT)Type] = State;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    auto CCurrentResourceState::GetExclusiveSubresourceState(UINT SubresourceIndex) const noexcept -> ExclusiveState const&
    {
        if (AreAllSubresourcesSame())
        {
            SubresourceIndex = 0;
        }
        return m_spExclusiveState[SubresourceIndex];
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    auto CCurrentResourceState::GetSharedSubresourceState(UINT SubresourceIndex) const noexcept -> SharedState const&
    {
        assert(!IsExclusiveState(SubresourceIndex));
        if (AreAllSubresourcesSame())
        {
            SubresourceIndex = 0;
        }
        return m_pSharedState[SubresourceIndex];
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT CCurrentResourceState::GetCommandListTypeMask() const noexcept
    {
        if (AreAllSubresourcesSame())
        {
            return GetCommandListTypeMask(0);
        }
        UINT TypeMask = 0;
        for (UINT i = 0; i < m_spExclusiveState.size(); ++i)
        {
            TypeMask |= GetCommandListTypeMask(i);
        }
        return TypeMask;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT CCurrentResourceState::GetCommandListTypeMask(CViewSubresourceSubset const & Subresources) const noexcept
    {
        if (AreAllSubresourcesSame())
        {
            return GetCommandListTypeMask(0);
        }
        UINT TypeMask = 0;
        for (auto range : Subresources)
        {
            for (UINT i = range.first; i < range.second; ++i)
            {
                TypeMask |= GetCommandListTypeMask(i);
            }
        }
        return TypeMask;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT CCurrentResourceState::GetCommandListTypeMask(UINT Subresource) const noexcept
    {
        if (IsExclusiveState(Subresource))
        {
            return (1 << (UINT)GetExclusiveSubresourceState(Subresource).CommandListType);
        }
        else
        {
            auto& SharedState = GetSharedSubresourceState(Subresource);
            UINT TypeMask = 0;
            for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
            {
                if (SharedState.FenceValues[i] > 0)
                {
                    TypeMask |= (1 << i);
                }
            }
            return TypeMask;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void CCurrentResourceState::Reset() noexcept
    {
        m_bAllSubresourcesSame = true;
        m_spExclusiveState[0] = ExclusiveState{};
        if (!m_pSharedState.empty())
        {
            m_pSharedState[0] = SharedState{};
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    ResourceStateManagerBase::ResourceStateManagerBase() noexcept(false)
    {
        D3D12TranslationLayer::InitializeListHead(&m_TransitionListHead);
        // Reserve some space in these vectors upfront. Values are arbitrary.
        for (auto& srcVec : m_vSrcResourceBarriers)
        {
            srcVec.reserve(50);
        }
        m_vDstResourceBarriers.reserve(50);
        m_vTentativeResourceBarriers.reserve(20);
        m_vPostApplyUpdates.reserve(50);
        m_SwapchainDeferredWaits.reserve(5);
        m_ResourceDeferredWaits.reserve(5);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    bool ShouldIgnoreTransitionRequest(CDesiredResourceState::SubresourceInfo const& CurrentState,
                                       CDesiredResourceState::SubresourceInfo const& NewState)
    {
        // If we're not supposed to transition for bindings, and we have an incoming request to transition to a binding state, ignore it.
        return (CurrentState.Flags & SubresourceTransitionFlags::NoBindingTransitions) != SubresourceTransitionFlags::None &&
               (NewState.Flags & SubresourceTransitionFlags::TransitionPreDraw) != SubresourceTransitionFlags::None;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::TransitionResource(TransitionableResourceBase& Resource,
                                                      CDesiredResourceState::SubresourceInfo const& State) noexcept
    {
        if (ShouldIgnoreTransitionRequest(Resource.m_DesiredState.GetSubresourceInfo(0), State))
        {
            return;
        }
        Resource.m_DesiredState.SetResourceState(State);
        if (!Resource.IsTransitionPending())
        {
            InsertHeadList(&m_TransitionListHead, &Resource.m_TransitionListEntry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::TransitionSubresources(TransitionableResourceBase& Resource,
                                                          CViewSubresourceSubset const& Subresources,
                                                          CDesiredResourceState::SubresourceInfo const& State) noexcept
    {
        if (Subresources.IsWholeResource())
        {
            if (ShouldIgnoreTransitionRequest(Resource.m_DesiredState.GetSubresourceInfo(0), State))
            {
                return;
            }
            Resource.m_DesiredState.SetResourceState(State);
        }
        else
        {
            for (auto&& range : Subresources)
            {
                for (UINT i = range.first; i < range.second; ++i)
                {
                    if (ShouldIgnoreTransitionRequest(Resource.m_DesiredState.GetSubresourceInfo(i), State))
                    {
                        continue;
                    }
                    Resource.m_DesiredState.SetSubresourceState(i, State);
                }
            }
        }
        if (!Resource.IsTransitionPending())
        {
            InsertHeadList(&m_TransitionListHead, &Resource.m_TransitionListEntry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::TransitionSubresource(TransitionableResourceBase& Resource,
                                                         UINT SubresourceIndex,
                                                         CDesiredResourceState::SubresourceInfo const& State) noexcept
    {
        Resource.m_DesiredState.SetSubresourceState(SubresourceIndex, State);
        if (!Resource.IsTransitionPending())
        {
            InsertHeadList(&m_TransitionListHead, &Resource.m_TransitionListEntry);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::AddDeferredWait(std::shared_ptr<Fence> const& spFence, UINT64 Value) noexcept(false)
    {
        m_SwapchainDeferredWaits.emplace_back(DeferredWait{ spFence, Value }); // throw( bad_alloc )
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::ApplyResourceTransitionsPreamble() noexcept
    {
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
        {
            m_vSrcResourceBarriers[i].clear();
        }
        m_vDstResourceBarriers.clear();
        m_vTentativeResourceBarriers.clear();
        m_vPostApplyUpdates.clear();

        m_DestinationCommandListType = COMMAND_LIST_TYPE::UNKNOWN;
        m_bApplySwapchainDeferredWaits = false;
        for (auto& bFlush : m_bFlushQueues) { bFlush = false; }
        for (auto& FenceValue : m_QueueFenceValuesToWaitOn) { FenceValue = 0; }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    /*static*/ bool ResourceStateManagerBase::TransitionRequired(D3D12_RESOURCE_STATES CurrentState, D3D12_RESOURCE_STATES& DestinationState, SubresourceTransitionFlags Flags) noexcept
    {
        // An exact match never needs a transition.
        if (CurrentState == DestinationState)
        {
            return false;
        }

        // Not an exact match, but an exact match required, so do the transition.
        if ((Flags & SubresourceTransitionFlags::StateMatchExact) != SubresourceTransitionFlags::None)
        {
            return true;
        }

        // Current state already contains the destination state, we're good.
        if ((CurrentState & DestinationState) == DestinationState)
        {
            DestinationState = CurrentState;
            return false;
        }

        // If the transition involves a write state, then the destination should just be the requested destination.
        // Otherwise, accumulate read states to minimize future transitions (by triggering the above condition).
        if (!IsD3D12WriteState(DestinationState, SubresourceTransitionFlags::None) &&
            !IsD3D12WriteState(CurrentState, SubresourceTransitionFlags::None))
        {
            DestinationState |= CurrentState;
        }
        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::AddCurrentStateUpdate(TransitionableResourceBase& Resource,
                                                         CCurrentResourceState& CurrentState,
                                                         UINT SubresourceIndex,
                                                         D3D12_RESOURCE_STATES NewState,
                                                         PostApplyExclusiveState ExclusiveState,
                                                         bool IsGoingToDestinationType) noexcept(false)
    {
        PostApplyUpdate Update =
        {
            Resource, CurrentState, SubresourceIndex, NewState, ExclusiveState, IsGoingToDestinationType
        };
        m_vPostApplyUpdates.push_back(Update); // throw( bad_alloc )
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    auto ResourceStateManagerBase::ProcessTransitioningResource(ID3D12Resource* pTransitioningResource,
                                                                TransitionableResourceBase& TransitionableResource,
                                                                CCurrentResourceState& CurrentState,
                                                                CResourceBindings& BindingState,
                                                                UINT NumTotalSubresources,
                                                                _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
                                                                bool bIsPreDraw) noexcept(false) -> TransitionResult
    {
        // By default, assume that the resource is fully processed by this routine.
        TransitionResult result = TransitionResult::Remove;

        // Figure out the set of subresources that are transitioning
        auto& DestinationState = TransitionableResource.m_DesiredState;
        bool bAllSubresourcesAtOnce = CurrentState.AreAllSubresourcesSame() && DestinationState.AreAllSubresourcesSame();
        bool bNeedsTransitionToBindState = false;

        D3D12_RESOURCE_BARRIER TransitionDesc;
        ZeroMemory(&TransitionDesc, sizeof(TransitionDesc));
        TransitionDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        TransitionDesc.Transition.pResource = pTransitioningResource;

        UINT numSubresources = bAllSubresourcesAtOnce ? 1 : NumTotalSubresources;
        for (UINT i = 0; i < numSubresources; ++i)
        {
            CDesiredResourceState::SubresourceInfo SubresourceDestinationInfo = DestinationState.GetSubresourceInfo(i);
            TransitionDesc.Transition.Subresource = bAllSubresourcesAtOnce ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : i;

            // Is this subresource relevant for the current transition?
            if ((SubresourceDestinationInfo.Flags & SubresourceTransitionFlags::TransitionPreDraw) != SubresourceTransitionFlags::None &&
                !bIsPreDraw)
            {
                // Nope, we'll go to the next subresource, and also indicate to leave this resource in the transition list so that
                // we come back to it on the next draw operation.
                result = TransitionResult::Keep;
                continue;
            }

            // Is this subresource currently being used, or is it just being iterated over?
            D3D12_RESOURCE_STATES after = SubresourceDestinationInfo.State;
            COMMAND_LIST_TYPE curCmdListType = SubresourceDestinationInfo.CommandListType;
            SubresourceTransitionFlags Flags = SubresourceDestinationInfo.Flags;
            if (after == UNKNOWN_RESOURCE_STATE ||
                (curCmdListType == COMMAND_LIST_TYPE::UNKNOWN &&
                 (Flags & SubresourceTransitionFlags::StateMatchExact) == SubresourceTransitionFlags::None))
            {
                // This subresource doesn't have any transition requested - move on to the next.
                continue;
            }

#if DBG
            std::vector<D3D12_RESOURCE_BARRIER>* pBarrierVectors[2 + _countof(m_vSrcResourceBarriers)] = { &m_vTentativeResourceBarriers, &m_vDstResourceBarriers };
            std::transform(m_vSrcResourceBarriers, std::end(m_vSrcResourceBarriers), &pBarrierVectors[2], [](auto& vec) { return &vec; });
            // This subresource should not already be in any transition list
            for (auto pVec : pBarrierVectors)
            {
                for (auto& desc : *pVec)
                {
                    assert(!(desc.Transition.pResource == pTransitioningResource &&
                             desc.Transition.Subresource == TransitionDesc.Transition.Subresource));
                }
            }
#endif

            // COMMAND_LIST_TYPE::UNKNOWN is only supported as a destination when transitioning to COMMON.
            // Additionally, all destination command list types processed in a single ApplyAll should
            // be going to the same command list type (if any).
            assert(m_DestinationCommandListType == COMMAND_LIST_TYPE::UNKNOWN ||
                   (curCmdListType == COMMAND_LIST_TYPE::UNKNOWN && after == D3D12_RESOURCE_STATE_COMMON) ||
                   m_DestinationCommandListType == curCmdListType);

            if (curCmdListType != COMMAND_LIST_TYPE::UNKNOWN)
            {
                // We will synchronize so that work submitted to this command list type
                // occurs after all in-flight references to transitioning resources.
                m_DestinationCommandListType = curCmdListType;
            }
            else
            {
                // We will transition this resource to COMMON on whatever command queue
                // it is currently exclusive on.
                curCmdListType = CurrentState.GetExclusiveSubresourceState(i).CommandListType;
            }

            // Check if we should leave this resource in the list and update its destination to match its bindings.
            if ((Flags & SubresourceTransitionFlags::NoBindingTransitions) == SubresourceTransitionFlags::None &&
                !bIsPreDraw)
            {
                D3D12_RESOURCE_STATES stateFromBindings = BindingState.GetD3D12ResourceUsageFromBindings(i);
                if (stateFromBindings != UNKNOWN_RESOURCE_STATE && after != stateFromBindings)
                {
                    bNeedsTransitionToBindState = true;
                }
            }
            // The NoBindingTransition flag should be set on the whole resource or not at all.
            assert((Flags & SubresourceTransitionFlags::NoBindingTransitions) == SubresourceTransitionFlags::None ||
                   !bNeedsTransitionToBindState);

            if (IsD3D12WriteState(after, Flags) && TransitionableResource.m_bTriggersSwapchainDeferredWaits)
            {
                m_bApplySwapchainDeferredWaits = true;
            }

            if (!TransitionableResource.m_ResourceDeferredWaits.empty())
            {
                m_ResourceDeferredWaits.insert(m_ResourceDeferredWaits.begin(),
                    TransitionableResource.m_ResourceDeferredWaits.begin(),
                    TransitionableResource.m_ResourceDeferredWaits.end()); // throw( bad_alloc )
                TransitionableResource.m_ResourceDeferredWaits.clear();
            }

            if (CurrentState.IsExclusiveState(i))
            {
                ProcessTransitioningSubresourceExclusive(
                    CurrentState,
                    i,
                    curCmdListType,
                    CurrentFenceValues,
                    SubresourceDestinationInfo,
                    after,
                    TransitionableResource,
                    TransitionDesc,
                    Flags); // throw( bad_alloc )
            }
            else
            {
                ProcessTransitioningSubresourceShared(
                    CurrentState,
                    i,
                    after,
                    Flags,
                    CurrentFenceValues,
                    curCmdListType,
                    TransitionDesc,
                    TransitionableResource); // throw( bad_alloc )
            }
        }

        CDesiredResourceState::SubresourceInfo UnknownDestinationState = {};
        UnknownDestinationState.CommandListType = COMMAND_LIST_TYPE::UNKNOWN;
        UnknownDestinationState.State = UNKNOWN_RESOURCE_STATE;
        UnknownDestinationState.Flags = SubresourceTransitionFlags::None;

        // Update destination states.
        if (bNeedsTransitionToBindState)
        {
            result = TransitionResult::Keep;
            bAllSubresourcesAtOnce = BindingState.AreAllSubresourcesTheSame();
            numSubresources = bAllSubresourcesAtOnce ? 1 : NumTotalSubresources;

            for (UINT i = 0; i < numSubresources; ++i)
            {
                CDesiredResourceState::SubresourceInfo BindingDestinationState = {};
                BindingDestinationState.CommandListType = COMMAND_LIST_TYPE::GRAPHICS;
                BindingDestinationState.State = BindingState.GetD3D12ResourceUsageFromBindings(i);
                BindingDestinationState.Flags = SubresourceTransitionFlags::TransitionPreDraw;
                if (bAllSubresourcesAtOnce)
                {
                    DestinationState.SetResourceState(BindingDestinationState);
                }
                else
                {
                    DestinationState.SetSubresourceState(i, BindingDestinationState);
                }
            }
        }
        else if (result == TransitionResult::Remove) // We're done
        {
            // Coalesce destination state to ensure that it's set for the entire resource.
            DestinationState.SetResourceState(UnknownDestinationState);
        }
        else if (!bIsPreDraw)
        {
            // There must be some subresource which was pending a draw transition, but not one that transitioned this time.
            // Make sure all *other* subresources have their pending transitions cleared.
            assert(!DestinationState.AreAllSubresourcesSame() ||
                (DestinationState.GetSubresourceInfo(0).Flags & SubresourceTransitionFlags::TransitionPreDraw) != SubresourceTransitionFlags::None);

            bAllSubresourcesAtOnce = DestinationState.AreAllSubresourcesSame();
            numSubresources = bAllSubresourcesAtOnce ? 1 : NumTotalSubresources;

            for (UINT i = 0; i < numSubresources; ++i)
            {
                if ((DestinationState.GetSubresourceInfo(i).Flags & SubresourceTransitionFlags::TransitionPreDraw) == SubresourceTransitionFlags::None)
                {
                    if (bAllSubresourcesAtOnce)
                    {
                        DestinationState.SetResourceState(UnknownDestinationState);
                    }
                    else
                    {
                        DestinationState.SetSubresourceState(i, UnknownDestinationState);
                    }
                }
            }
        }

        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::ProcessTransitioningSubresourceExclusive(
        CCurrentResourceState& CurrentState,
        UINT i,
        COMMAND_LIST_TYPE curCmdListType,
        _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
        CDesiredResourceState::SubresourceInfo& SubresourceDestinationInfo,
        D3D12_RESOURCE_STATES after,
        TransitionableResourceBase& TransitionableResource,
        D3D12_RESOURCE_BARRIER& TransitionDesc,
        SubresourceTransitionFlags Flags) noexcept(false)
    {
        // If the subresource is currently exclusively owned by a queue, there's one of several outcomes:
        // 1. A transition barrier on the queue which owns it into a different state.
        //    For simultaneous access resources, this only happens if the new usage is in the same command list as the previous.
        //    Otherwise, the resource switches to shared ownership.
        // 2. Non-simultaneous access only: Changing queue ownership. This results in a barrier to common on the source (if not already COMMON),
        //    and a transition to the destination state on the destination queue (if not COMMON), after a flush on each and sync.
        //    If no barrier was submitted to the source, then the dest will wait on the fence which was stored when the subresource transitioned to COMMON.
        // 3. Simultaneous access only: Changing queue ownership. This results in flushing the source (if used in the current command list) 
        //    and dest (if not empty) with sync.
        //    The subresource remains exclusively owned by the new queue IFF the new state is a write state, otherwise it becomes shared.
        // 4. Simultaneous access only: Not changing queue ownership, and not used in the current command list.
        //    The subresource simply transitions from exclusive to shared state if the new state is a read state.
        CCurrentResourceState::ExclusiveState CurrentExclusiveState = CurrentState.GetExclusiveSubresourceState(i);
        PostApplyExclusiveState PostExclusiveState = PostApplyExclusiveState::Exclusive;

        auto FlushOrUpdateFenceValue = [&]()
        {
            if (CurrentExclusiveState.CommandListType == COMMAND_LIST_TYPE::UNKNOWN)
            {
                // The only time we should have an exclusive state with UNKNOWN command list type
                // is for resources that haven't been used yet.
                return;
            }

            if (CurrentFenceValues[(UINT)CurrentExclusiveState.CommandListType] == CurrentExclusiveState.FenceValue)
            {
                m_bFlushQueues[(UINT)CurrentExclusiveState.CommandListType] = true;
            }
            else
            {
                m_QueueFenceValuesToWaitOn[(UINT)CurrentExclusiveState.CommandListType] =
                    std::max(m_QueueFenceValuesToWaitOn[(UINT)CurrentExclusiveState.CommandListType],
                             CurrentExclusiveState.FenceValue);
            }
        };

        bool bQueueStateUpdate = (SubresourceDestinationInfo.Flags & SubresourceTransitionFlags::NotUsedInCommandListIfNoStateChange) == SubresourceTransitionFlags::None;

        if (curCmdListType == CurrentExclusiveState.CommandListType &&
            (!CurrentState.SupportsSimultaneousAccess() ||
             CurrentFenceValues[(UINT)curCmdListType] == CurrentExclusiveState.FenceValue))
        {

            if (TransitionRequired(CurrentExclusiveState.State, /*inout*/ after, SubresourceDestinationInfo.Flags))
            {
                // Case 1: Insert a single concrete barrier.
                // Note: For simultaneous access resources, barriers go into a tentative list
                // because further barrier processing may cause us to flush this command queue,
                // making all simultaneous access resource states decay and then implicitly promote.
                // For resources which trigger deferred waits, they go into a vector which is only safe to submit
                // after a flush.
                // All other resources go into a vector which is safe to submit before a flush.
                auto& TransitionVector = CurrentState.SupportsSimultaneousAccess() ?
                    m_vTentativeResourceBarriers :
                    (TransitionableResource.m_bTriggersSwapchainDeferredWaits ?
                     m_vDstResourceBarriers : m_vSrcResourceBarriers[(UINT)curCmdListType]);
                TransitionDesc.Transition.StateBefore = D3D12_RESOURCE_STATES(CurrentExclusiveState.State);
                TransitionDesc.Transition.StateAfter = D3D12_RESOURCE_STATES(after);
                assert(TransitionDesc.Transition.StateBefore != TransitionDesc.Transition.StateAfter);
                TransitionVector.push_back(TransitionDesc); // throw( bad_alloc )

                PostExclusiveState = (CurrentState.SupportsSimultaneousAccess() && !IsD3D12WriteState(after, Flags)) ?
                    PostApplyExclusiveState::SharedIfFlushed : PostApplyExclusiveState::Exclusive;
                bQueueStateUpdate = true;
            }
            // Regardless of whether a transition was inserted, we'll update the current state
            // of this subresource, to at least update its fence value.
            // Unless it was transitioning to COMMON for CPU access and was already in COMMON.
        }
        else if (curCmdListType != CurrentExclusiveState.CommandListType)
        {
            if (!CurrentState.SupportsSimultaneousAccess())
            {
                // Case 2: Changing queue ownership with concrete barriers.
                if (CurrentExclusiveState.State == D3D12_RESOURCE_STATE_COMMON)
                {
                    FlushOrUpdateFenceValue();
                }
                else
                {
                    TransitionDesc.Transition.StateBefore = D3D12_RESOURCE_STATES(CurrentExclusiveState.State);
                    TransitionDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
                    m_vSrcResourceBarriers[(UINT)CurrentExclusiveState.CommandListType].push_back(TransitionDesc); // throw( bad_alloc )
                }

                if (after != D3D12_RESOURCE_STATE_COMMON)
                {
                    // TODO: Don't do this for SRV or copy src/dest.
                    TransitionDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                    TransitionDesc.Transition.StateAfter = D3D12_RESOURCE_STATES(after);
                    m_vDstResourceBarriers.push_back(TransitionDesc); // throw( bad_alloc )
                    bQueueStateUpdate = true;
                }
            }
            else
            {
                // Case 3: Changing queue ownership via promotion/decay.
                FlushOrUpdateFenceValue();
                PostExclusiveState = IsD3D12WriteState(after, Flags) ?
                    PostApplyExclusiveState::Exclusive : PostApplyExclusiveState::Shared;
            }
        }
        else if (CurrentState.SupportsSimultaneousAccess())
        {
            assert(CurrentFenceValues[(UINT)curCmdListType] != CurrentExclusiveState.FenceValue);
            PostExclusiveState = IsD3D12WriteState(after, Flags) ?
                PostApplyExclusiveState::Exclusive : PostApplyExclusiveState::Shared;
        }

        if (bQueueStateUpdate)
        {
            AddCurrentStateUpdate(TransitionableResource,
                                  CurrentState,
                                  TransitionDesc.Transition.Subresource,
                                  after,
                                  PostExclusiveState,
                                  SubresourceDestinationInfo.CommandListType != COMMAND_LIST_TYPE::UNKNOWN); // throw( bad_alloc )
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::ProcessTransitioningSubresourceShared(
        CCurrentResourceState& CurrentState,
        UINT i,
        D3D12_RESOURCE_STATES after,
        SubresourceTransitionFlags Flags,
        _In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) const UINT64* CurrentFenceValues,
        COMMAND_LIST_TYPE curCmdListType,
        D3D12_RESOURCE_BARRIER& TransitionDesc,
        TransitionableResourceBase& TransitionableResource) noexcept(false)
    {
        bool bQueueStateUpdate = (Flags & SubresourceTransitionFlags::NotUsedInCommandListIfNoStateChange) == SubresourceTransitionFlags::None;

        // If the subresource is not currently exclusively owned by a queue, then it is a simultaneous access resource.
        // There are several possible outcomes here as well:
        // 1. The resource is being written to, and transitions into being exclusively owned by a queue.
        //    Any previous shared owners are flushed, along with the new exclusive owner if there were other previous owners.
        //    If the only previous owner was the same as the new owner, and in the same command list, a concrete barrier is inserted.
        // 2. The resource is not being written to. The shared state is simply updated. No barriers are issued.
        assert(CurrentState.SupportsSimultaneousAccess());
        auto& SharedState = CurrentState.GetSharedSubresourceState(i);
        PostApplyExclusiveState PostExclusiveState = PostApplyExclusiveState::Shared;
        if (IsD3D12WriteState(after, Flags))
        {
            PostExclusiveState = PostApplyExclusiveState::Exclusive;
            for (UINT CommandListType = 0; CommandListType < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++CommandListType)
            {
                assert(!IsD3D12WriteState(SharedState.State[CommandListType], SubresourceTransitionFlags::None));
                if (SharedState.FenceValues[CommandListType] == CurrentFenceValues[CommandListType])
                {
                    if (CommandListType == (UINT)curCmdListType)
                    {
                        // Only prep a barrier if it's a real write state.
                        if (IsD3D12WriteState(after, SubresourceTransitionFlags::None))
                        {
                            TransitionDesc.Transition.StateBefore = D3D12_RESOURCE_STATES(SharedState.State[CommandListType]);
                            TransitionDesc.Transition.StateAfter = D3D12_RESOURCE_STATES(after);
                            assert(TransitionDesc.Transition.StateBefore != TransitionDesc.Transition.StateAfter);
                            m_vTentativeResourceBarriers.push_back(TransitionDesc); // throw( bad_alloc )
                            bQueueStateUpdate = true;
                        }
                    }
                    else
                    {
                        m_bFlushQueues[CommandListType] = true;
                    }
                }
                else if (CommandListType != (UINT)m_DestinationCommandListType)
                {
                    m_QueueFenceValuesToWaitOn[CommandListType] =
                        std::max(m_QueueFenceValuesToWaitOn[CommandListType],
                                 SharedState.FenceValues[CommandListType]);
                }
            }
        }
        else
        {
            after |= SharedState.State[(UINT)m_DestinationCommandListType];
            assert(!IsD3D12WriteState(after, SubresourceTransitionFlags::None));

            auto& ExclusiveState = CurrentState.GetExclusiveSubresourceState(i);
            if (ExclusiveState.CommandListType != COMMAND_LIST_TYPE::UNKNOWN &&
                ExclusiveState.CommandListType != m_DestinationCommandListType)
            {
                m_QueueFenceValuesToWaitOn[(UINT)ExclusiveState.CommandListType] =
                    std::max(m_QueueFenceValuesToWaitOn[(UINT)ExclusiveState.CommandListType],
                            ExclusiveState.FenceValue);
            }
        }

        if (bQueueStateUpdate)
        {
            AddCurrentStateUpdate(TransitionableResource,
                                  CurrentState,
                                  TransitionDesc.Transition.Subresource,
                                  after,
                                  PostExclusiveState,
                                  curCmdListType != COMMAND_LIST_TYPE::UNKNOWN); // throw( bad_alloc )
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::SubmitResourceBarriers(_In_reads_(Count) D3D12_RESOURCE_BARRIER const * pBarriers, UINT Count, _In_ CommandListManager * pManager) noexcept
    {
        switch (pManager->GetCommandListType())
        {
        case COMMAND_LIST_TYPE::GRAPHICS:
            pManager->GetGraphicsCommandList()->ResourceBarrier(Count, pBarriers);
            break;

        case COMMAND_LIST_TYPE::VIDEO_DECODE:
            pManager->GetVideoDecodeCommandList()->ResourceBarrier(Count, pBarriers);
            break;

        case COMMAND_LIST_TYPE::VIDEO_PROCESS:
            pManager->GetVideoProcessCommandList()->ResourceBarrier(Count, pBarriers);
            break;

        default:
            static_assert((UINT)COMMAND_LIST_TYPE::MAX_VALID == 3, "Need to update switch/case.");
            assert(false);
            break;
        }
        pManager->AdditionalCommandsAdded();
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    template <
        typename TSubmitBarriersImpl,
        typename TSubmitCmdListImpl,
        typename THasCommandsImpl,
        typename TGetCurrentFenceImpl,
        typename TInsertDeferredWaitsImpl,
        typename TInsertQueueWaitImpl
    > void ResourceStateManagerBase::SubmitResourceTransitionsImpl(
        TSubmitBarriersImpl&& SubmitBarriersImpl,
        TSubmitCmdListImpl&& SubmitCmdListImpl,
        THasCommandsImpl&& HasCommandsImpl,
        TGetCurrentFenceImpl&& GetCurrentFenceImpl,
        TInsertDeferredWaitsImpl&& InsertDeferredWaitsImpl,
        TInsertQueueWaitImpl&& InsertQueueWaitImpl)
    {

        // Step 1: Submit any pending barrires on source command lists that are not the destination.
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            if (!m_vSrcResourceBarriers[i].empty())
            {
                if (i == (UINT)m_DestinationCommandListType)
                {
                    // We'll get to it later.
                    continue;
                }

                // Submitting any barriers on a command list indicates it needs to be submitted before we're done.
                m_bFlushQueues[i] = true;
                SubmitBarriersImpl(m_vSrcResourceBarriers[i], (COMMAND_LIST_TYPE)i);
            }
        }

        if (m_DestinationCommandListType == COMMAND_LIST_TYPE::UNKNOWN)
        {
            // We've done all the necessary work.
            return;
        }

        // Step 2: Flush any necessary source command lists.
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            if (m_bFlushQueues[i])
            {
                assert(i != (UINT)m_DestinationCommandListType);
                // Submitting a command list indicates it needs to be waited on before we're done.
                m_QueueFenceValuesToWaitOn[i] = GetCurrentFenceImpl((COMMAND_LIST_TYPE)i);
                SubmitCmdListImpl((COMMAND_LIST_TYPE)i);
            }
        }

        // Step 3: Flush the destination command list type if necessary
        bool bFlushDestination = (m_bApplySwapchainDeferredWaits && !m_SwapchainDeferredWaits.empty()) ||
            !m_ResourceDeferredWaits.empty();
        if (!bFlushDestination)
        {
            for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
            {
                if (m_InsertedQueueSync[(UINT)m_DestinationCommandListType][i] < m_QueueFenceValuesToWaitOn[i])
                {
                    bFlushDestination = true;
                    break;
                }
            }
        }

        auto& SrcVec = m_vSrcResourceBarriers[(UINT)m_DestinationCommandListType];
        if (bFlushDestination && HasCommandsImpl(m_DestinationCommandListType))
        {
            // Submit any barriers that should go before the flush, if we have any.
            if (!SrcVec.empty())
            {
                // TODO: Consider converting these into split barriers and putting ends in m_vDstResourceBarriers.
                SubmitBarriersImpl(SrcVec, m_DestinationCommandListType);
            }

            SubmitCmdListImpl(m_DestinationCommandListType);
        }
        else
        {
            // If we didn't flush, then these barriers on simultaneous access
            // resources need to be done concretely.
            m_vDstResourceBarriers.insert(m_vDstResourceBarriers.end(),
                                          m_vTentativeResourceBarriers.begin(),
                                          m_vTentativeResourceBarriers.end()); // throw( bad_alloc )
                                                                               // And we'll add in the other concrete barriers that we haven't done yet.
            m_vDstResourceBarriers.insert(m_vDstResourceBarriers.end(),
                                          SrcVec.begin(),
                                          SrcVec.end()); // throw( bad_alloc )
        }

        // Step 4: Insert sync
        InsertDeferredWaitsImpl(m_DestinationCommandListType);
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            if (m_InsertedQueueSync[(UINT)m_DestinationCommandListType][i] < m_QueueFenceValuesToWaitOn[i])
            {
                InsertQueueWaitImpl(m_QueueFenceValuesToWaitOn[i], (COMMAND_LIST_TYPE)i, m_DestinationCommandListType);
                m_InsertedQueueSync[(UINT)m_DestinationCommandListType][i] = m_QueueFenceValuesToWaitOn[i];
            }
        }

        // Step 5: Insert destination barriers
        if (!m_vDstResourceBarriers.empty())
        {
            SubmitBarriersImpl(m_vDstResourceBarriers, m_DestinationCommandListType);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::SubmitResourceTransitions(_In_reads_((UINT)COMMAND_LIST_TYPE::MAX_VALID) CommandListManager ** ppManagers) noexcept(false)
    {
        auto SubmitBarriersImpl =
            [this, ppManagers](std::vector<D3D12_RESOURCE_BARRIER>& barriers, COMMAND_LIST_TYPE type)
        {
            this->SubmitResourceBarriers(barriers.data(), (UINT)barriers.size(), ppManagers[(UINT)type]);
        };
        auto SubmitCmdListImpl =
            [this, ppManagers](COMMAND_LIST_TYPE type)
        {
            // Note: Command list may not have pending commands, but queue may have commands which need to have a signal inserted.
            ppManagers[(UINT)type]->PrepForCommandQueueSync(); // throws
        };
        auto HasCommandsImpl =
            [this, ppManagers](COMMAND_LIST_TYPE type)
        {
            return ppManagers[(UINT)type]->ShouldFlushForResourceAcquire();
        };
        auto GetCurrentFenceImpl =
            [this, ppManagers](COMMAND_LIST_TYPE type)
        {
            return ppManagers[(UINT)type]->GetCommandListID();
        };
        auto InsertDeferredWaitsImpl =
            [this, ppManagers](COMMAND_LIST_TYPE type)
        {
            auto pDestinationManager = ppManagers[(UINT)type];
            if (m_bApplySwapchainDeferredWaits)
            {
                for (auto& Wait : m_SwapchainDeferredWaits)
                {
                    pDestinationManager->GetCommandQueue()->Wait(Wait.fence->Get(), Wait.value);
                    Wait.fence->UsedInCommandList(pDestinationManager->GetCommandListType(), pDestinationManager->GetCommandListID());
                }
                m_SwapchainDeferredWaits.clear();
            }
            for (auto& Wait : m_ResourceDeferredWaits)
            {
                pDestinationManager->GetCommandQueue()->Wait(Wait.fence->Get(), Wait.value);
                Wait.fence->UsedInCommandList(pDestinationManager->GetCommandListType(), pDestinationManager->GetCommandListID());
            }
            m_ResourceDeferredWaits.clear();
        };
        auto InsertQueueWaitImpl =
            [this, ppManagers](UINT64 value, COMMAND_LIST_TYPE src, COMMAND_LIST_TYPE dst)
        {
            if(src == dst || value <= ppManagers[(UINT)src]->GetFence()->GetCompletedValue())
            {
                return;
            }
            ppManagers[(UINT)dst]->GetCommandQueue()->Wait(ppManagers[(UINT)src]->GetFence()->Get(), value);
        };
        SubmitResourceTransitionsImpl(SubmitBarriersImpl, SubmitCmdListImpl, HasCommandsImpl, GetCurrentFenceImpl, InsertDeferredWaitsImpl, InsertQueueWaitImpl);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManagerBase::SimulateSubmitResourceTransitions(
        std::function<void(std::vector<D3D12_RESOURCE_BARRIER>&, COMMAND_LIST_TYPE)> SubmitBarriers,
        std::function<void(COMMAND_LIST_TYPE)> SubmitCmdList,
        std::function<bool(COMMAND_LIST_TYPE)> HasCommands,
        std::function<UINT64(COMMAND_LIST_TYPE)> GetCurrentFenceImpl,
        std::function<void(COMMAND_LIST_TYPE)> InsertDeferredWaits,
        std::function<void(UINT64, COMMAND_LIST_TYPE Src, COMMAND_LIST_TYPE Dst)> InsertQueueWait)
    {
        SubmitResourceTransitionsImpl(SubmitBarriers, SubmitCmdList, HasCommands, GetCurrentFenceImpl, InsertDeferredWaits, InsertQueueWait);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::TransitionResource(Resource* pResource, D3D12_RESOURCE_STATES State, COMMAND_LIST_TYPE DestinationCommandListType, SubresourceTransitionFlags Flags) noexcept
    {
        CDesiredResourceState::SubresourceInfo DesiredState = { State, DestinationCommandListType, Flags };
        ResourceStateManagerBase::TransitionResource(*pResource, DesiredState);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::TransitionSubresources(Resource* pResource, CViewSubresourceSubset const & Subresources, D3D12_RESOURCE_STATES State, COMMAND_LIST_TYPE DestinationCommandListType, SubresourceTransitionFlags Flags) noexcept
    {
        CDesiredResourceState::SubresourceInfo DesiredState = { State, DestinationCommandListType, Flags };
        ResourceStateManagerBase::TransitionSubresources(*pResource, Subresources, DesiredState);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::TransitionSubresource(Resource* pResource, UINT SubresourceIndex, D3D12_RESOURCE_STATES State, COMMAND_LIST_TYPE DestinationCommandListType, SubresourceTransitionFlags Flags) noexcept
    {
        CDesiredResourceState::SubresourceInfo DesiredState = { State, DestinationCommandListType, Flags };
        ResourceStateManagerBase::TransitionSubresource(*pResource, SubresourceIndex, DesiredState);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::TransitionResourceForBindings(Resource* pResource) noexcept
    {
        auto& Bindings = pResource->GetBindingState();
        if (Bindings.AreAllSubresourcesTheSame())
        {
            TransitionResource(pResource, Bindings.GetD3D12ResourceUsageFromBindings(0), COMMAND_LIST_TYPE::GRAPHICS, SubresourceTransitionFlags::TransitionPreDraw);
        }
        else
        {
            for (UINT i = 0; i < pResource->NumSubresources(); ++i)
            {
                TransitionSubresource(pResource, i, Bindings.GetD3D12ResourceUsageFromBindings(i), COMMAND_LIST_TYPE::GRAPHICS, SubresourceTransitionFlags::TransitionPreDraw);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::TransitionSubresourcesForBindings(Resource* pResource, CViewSubresourceSubset const & Subresources) noexcept
    {
        auto& Bindings = pResource->GetBindingState();
        if (Bindings.AreAllSubresourcesTheSame())
        {
            TransitionResource(pResource, Bindings.GetD3D12ResourceUsageFromBindings(0), COMMAND_LIST_TYPE::GRAPHICS, SubresourceTransitionFlags::TransitionPreDraw);
        }
        else
        {
            for (auto range : Subresources)
            {
                for (UINT i = range.first; i < range.second; ++i)
                {
                    TransitionSubresource(pResource, i, Bindings.GetD3D12ResourceUsageFromBindings(i), COMMAND_LIST_TYPE::GRAPHICS, SubresourceTransitionFlags::TransitionPreDraw);
                }
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void ResourceStateManager::ApplyAllResourceTransitions(bool bIsPreDraw) noexcept(false)
    {
        CommandListManager* pCommandListManagers[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        UINT64 CurrentFenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID];
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
        {
            pCommandListManagers[i] = m_ImmCtx.GetCommandListManager((COMMAND_LIST_TYPE)i);
            CurrentFenceValues[i] = pCommandListManagers[i] ? pCommandListManagers[i]->GetCommandListID() : 1ull;
        }

        ApplyResourceTransitionsPreamble();
        ForEachTransitioningResource([=](TransitionableResourceBase& ResourceBase) -> TransitionResult
        {
            Resource& CurResource = static_cast<Resource&>(ResourceBase);

            return ProcessTransitioningResource(
                CurResource.GetUnderlyingResource(),
                CurResource,
                CurResource.GetIdentity()->m_currentState,
                CurResource.GetBindingState(),
                CurResource.NumSubresources(),
                CurrentFenceValues,
                bIsPreDraw); // throw( bad_alloc )
        });

        SubmitResourceTransitions(pCommandListManagers); // throw( bad_alloc )

        UINT64 NewCommandListID = 0ull;
        if (m_DestinationCommandListType != COMMAND_LIST_TYPE::UNKNOWN)
        {
            NewCommandListID = pCommandListManagers[(UINT)m_DestinationCommandListType]->GetCommandListID();
        }
        PostSubmitUpdateState([=](PostApplyUpdate const& update, COMMAND_LIST_TYPE CmdListType, UINT64 FenceValue)
        {
            static_cast<Resource&>(update.AffectedResource).UsedInCommandList(CmdListType, FenceValue);
            m_ImmCtx.GetCommandListManager(CmdListType)->SetNeedSubmitFence();
        }, CurrentFenceValues, NewCommandListID);
    }
};

