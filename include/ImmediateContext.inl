// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once
namespace D3D12TranslationLayer
{

//----------------------------------------------------------------------------------------------------------------------------------
template<> inline LIST_ENTRY& CResourceBindings::GetViewList<ShaderResourceViewType>() { return m_ShaderResourceViewList; }
template<> inline LIST_ENTRY& CResourceBindings::GetViewList<RenderTargetViewType>() { return m_RenderTargetViewList; }
template<> inline LIST_ENTRY& CResourceBindings::GetViewList<UnorderedAccessViewType>() { return m_UnorderedAccessViewList; }

template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetBindFunc<ShaderResourceViewType>(EShaderStage stage)
{
    return (stage == e_PS) ?
                       &CSubresourceBindings::PixelShaderResourceViewBound :
                       &CSubresourceBindings::NonPixelShaderResourceViewBound;
}
template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetBindFunc<RenderTargetViewType>(EShaderStage) { return &CSubresourceBindings::RenderTargetViewBound; }
template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetBindFunc<UnorderedAccessViewType>(EShaderStage) { return &CSubresourceBindings::UnorderedAccessViewBound; }

template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetUnbindFunc<ShaderResourceViewType>(EShaderStage stage)
{
    return (stage == e_PS) ?
                       &CSubresourceBindings::PixelShaderResourceViewUnbound :
                       &CSubresourceBindings::NonPixelShaderResourceViewUnbound;
}
template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetUnbindFunc<RenderTargetViewType>(EShaderStage) { return &CSubresourceBindings::RenderTargetViewUnbound; }
template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetUnbindFunc<DepthStencilViewType>(EShaderStage) { return &CSubresourceBindings::DepthStencilViewUnbound; }
template<> inline CSubresourceBindings::BindFunc CResourceBindings::GetUnbindFunc<UnorderedAccessViewType>(EShaderStage) { return &CSubresourceBindings::UnorderedAccessViewUnbound; }

//----------------------------------------------------------------------------------------------------------------------------------
template<typename TIface>
void CResourceBindings::ViewBound(View<TIface>* pView, EShaderStage stage, UINT /*slot*/)
{
    auto& viewBindings = pView->m_currentBindings;
    pView->IncrementBindRefs();
    if (!viewBindings.IsViewBound())
    {
        D3D12TranslationLayer::InsertHeadList(&GetViewList<TIface>(), &viewBindings.m_ViewBindingList);
    }
    CViewSubresourceSubset &viewSubresources = pView->m_subresources;
    ViewBoundCommon(viewSubresources, GetBindFunc<TIface>(stage));
}

//----------------------------------------------------------------------------------------------------------------------------------
template<typename TIface>
void CResourceBindings::ViewUnbound(View<TIface>* pView, EShaderStage stage, UINT /*slot*/)
{
    auto& viewBindings = pView->m_currentBindings;
    pView->DecrementBindRefs();
    if (pView->GetBindRefs() == 0 && viewBindings.IsViewBound())
    {
        D3D12TranslationLayer::RemoveEntryList(&viewBindings.m_ViewBindingList);
        D3D12TranslationLayer::InitializeListHead(&viewBindings.m_ViewBindingList);
    }
    CViewSubresourceSubset &viewSubresources = pView->m_subresources;
    ViewUnboundCommon(viewSubresources, GetUnbindFunc<TIface>(stage));
}

//----------------------------------------------------------------------------------------------------------------------------------
template<> inline void CResourceBindings::ViewBound<DepthStencilViewType>(TDSV* pView, EShaderStage, UINT /*slot*/)
{
    assert(!m_bIsDepthStencilViewBound);
    m_bIsDepthStencilViewBound = true;
    pView->IncrementBindRefs();

    CViewSubresourceSubset &viewSubresources = pView->m_subresources;
    
    bool bHasStencil = pView->m_pResource->SubresourceMultiplier() != 1;
    bool bReadOnlyDepth = !!(pView->GetDesc12().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
    bool bReadOnlyStencil = !!(pView->GetDesc12().Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL);
    auto pfnDepthBound = bReadOnlyDepth ? &CSubresourceBindings::ReadOnlyDepthStencilViewBound : &CSubresourceBindings::WritableDepthStencilViewBound;
    if (!bHasStencil || bReadOnlyDepth == bReadOnlyStencil)
    {
        ViewBoundCommon(viewSubresources, pfnDepthBound);
        return;
    }

    CViewSubresourceSubset readSubresources(pView->GetDesc12(),
                                             pView->m_pResource->AppDesc()->MipLevels(),
                                             pView->m_pResource->AppDesc()->ArraySize(),
                                             pView->m_pResource->SubresourceMultiplier(),
                                             CViewSubresourceSubset::ReadOnly);
    CViewSubresourceSubset writeSubresources(pView->GetDesc12(),
                                             pView->m_pResource->AppDesc()->MipLevels(),
                                             pView->m_pResource->AppDesc()->ArraySize(),
                                             pView->m_pResource->SubresourceMultiplier(),
                                             CViewSubresourceSubset::WriteOnly);

    // If either of these were empty, then there would be only one type of bind required, and the (readOnlyDepth == readOnlyStencil) check would've covered it
    assert(!readSubresources.IsEmpty() && !writeSubresources.IsEmpty());

    UINT NumViewsReferencingSubresources = m_NumViewsReferencingSubresources;
    ViewBoundCommon(readSubresources, &CSubresourceBindings::ReadOnlyDepthStencilViewBound);
    ViewBoundCommon(writeSubresources, &CSubresourceBindings::WritableDepthStencilViewBound);
    m_NumViewsReferencingSubresources = NumViewsReferencingSubresources + 1;
}

//----------------------------------------------------------------------------------------------------------------------------------
template<> inline void CResourceBindings::ViewUnbound<DepthStencilViewType>(TDSV* pView, EShaderStage stage, UINT /*slot*/)
{
#if TRANSLATION_LAYER_DBG
    // View bindings aren't used for DSVs
    auto& viewBindings = pView->m_currentBindings;
    assert(!viewBindings.IsViewBound());
#endif

    assert(m_bIsDepthStencilViewBound);
    m_bIsDepthStencilViewBound = false;
    pView->DecrementBindRefs();

    CViewSubresourceSubset &viewSubresources = pView->m_subresources;
    ViewUnboundCommon(viewSubresources, GetUnbindFunc<DepthStencilViewType>(stage));
}

//----------------------------------------------------------------------------------------------------------------------------------
// Binding helpers
//----------------------------------------------------------------------------------------------------------------------------------
inline void VBBinder::Bound(Resource* pBuffer, UINT slot, EShaderStage)  { return ImmediateContext::VertexBufferBound(pBuffer, slot); }
inline void VBBinder::Unbound(Resource* pBuffer, UINT slot, EShaderStage) { return ImmediateContext::VertexBufferUnbound(pBuffer, slot); }
inline void IBBinder::Bound(Resource* pBuffer, UINT, EShaderStage)  { return ImmediateContext::IndexBufferBound(pBuffer); }
inline void IBBinder::Unbound(Resource* pBuffer, UINT, EShaderStage) { return ImmediateContext::IndexBufferUnbound(pBuffer); }
inline void SOBinder::Bound(Resource* pBuffer, UINT slot, EShaderStage)  { return ImmediateContext::StreamOutputBufferBound(pBuffer, slot); }
inline void SOBinder::Unbound(Resource* pBuffer, UINT slot, EShaderStage) { return ImmediateContext::StreamOutputBufferUnbound(pBuffer, slot); }

//----------------------------------------------------------------------------------------------------------------------------------
inline bool BitSetLessThan(unsigned long bits, UINT slot)
{
    unsigned long index = 0;
    return BitScanForward(&index, bits) && index < slot;
}
inline bool BitSetLessThan(unsigned long long bits, UINT slot)
{
#ifdef BitScanForward64
    unsigned long index = 0;
    return BitScanForward64(&index, bits) && index < slot;
#else
    if (slot < 32)
    {
        return BitSetLessThan(static_cast<unsigned long>(bits), slot);
    }
    if (static_cast<unsigned long>(bits))
    {
        return true;
    }
    return BitSetLessThan(static_cast<unsigned long>(bits >> 32), slot - 32);
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TBindable, UINT NumBindSlots>
inline bool CBoundState<TBindable, NumBindSlots>::DirtyBitsUpTo(_In_range_(0, NumBindings) UINT NumBitsToCheck) const noexcept
{
    if (NumBitsToCheck == 0)
    {
        return false;
    }

    if (NumBindSlots <= 32)
    {
        return BitSetLessThan(m_DirtyBits.to_ulong(), NumBitsToCheck);
    }
    else if (NumBindSlots <= 64)
    {
        return BitSetLessThan(m_DirtyBits.to_ullong(), NumBitsToCheck);
    }
    else
    {
        constexpr UINT NumBitsPerWord = sizeof(::std::conditional_t<NumBindings <= sizeof(unsigned long) * CHAR_BIT, unsigned long, unsigned long long>) * 8;
        // First, check whole "words" for any bit being set.
        UINT NumWordsToCheck = NumBitsToCheck / NumBitsPerWord;
        for (UINT word = 0; word < NumWordsToCheck; ++word)
        {
            if (m_DirtyBits._Getword(word))
            {
                return true;
            }
            NumBitsToCheck -= NumBitsPerWord;
        }
        // The slot we were asking about was the last bit of the last word we checked.
        if (NumBitsToCheck == 0)
        {
            return false;
        }
        // Check for bits inside a word.
        return BitSetLessThan(m_DirtyBits._Getword(NumWordsToCheck), NumBitsToCheck);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TBindable, UINT NumBindSlots>
void CBoundState<TBindable, NumBindSlots>::ReassertResourceState() const noexcept
{
    for (UINT i = 0; i < m_NumBound; ++i)
    {
        if (m_Bound[i])
        {
            ImmediateContext* pDevice = m_Bound[i]->m_pParent;
            pDevice->TransitionResourceForBindings(m_Bound[i]);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TBindable, UINT NumBindSlots, typename TBinder>
inline bool CSimpleBoundState<TBindable, NumBindSlots, TBinder>::UpdateBinding(_In_range_(0, NumBindings-1) UINT slot, _In_opt_ TBindable* pBindable, EShaderStage stage) noexcept
{
    auto pCurrent = this->m_Bound[slot];
    if (__super::UpdateBinding(slot, pBindable))
    {
        TBinder::Unbound(pCurrent, slot, stage);
        TBinder::Bound(pBindable, slot, stage);
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TBindable, UINT NumBindSlots>
inline bool CViewBoundState<TBindable, NumBindSlots>::UpdateBinding(_In_range_(0, NumBindings-1) UINT slot, _In_opt_ TBindable* pBindable, EShaderStage stage) noexcept
{
    auto& Current = this->m_Bound[slot];
    if (pBindable)
    {
        this->m_NumBound = max(this->m_NumBound, slot + 1);
    }
    if (Current != pBindable)
    {
        if (Current) Current->ViewUnbound(slot, stage);
        if (pBindable) pBindable->ViewBound(slot, stage);
        Current = pBindable;
        
        // We skip calling TrimNumBound because we just use shader data to determine the actual amount to bind
        this->m_DirtyBits.set(slot);
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TBindable, UINT NumBindSlots>
inline bool CViewBoundState<TBindable, NumBindSlots>::IsDirty(TDeclVector const& New, UINT rootSignatureBucketSize, bool bKnownDirty) noexcept
{
    // Note: Even though there are vector resize ops here, they cannot throw,
    // since the backing memory for the vector was already allocated using reserve(NumBindSlots)
    bool bDirty = bKnownDirty;
    for (UINT i = 0; i < New.size(); ++i)
    {
        if (i >= m_ShaderData.size())
        {
            // We've never bound this many before
            m_ShaderData.insert(m_ShaderData.end(), New.begin() + i, New.end());
            bDirty = true;
            break;
        }
        // Don't overwrite typed NULLs with untyped NULLs,
        // any type will work to fill a slot that won't be used
        if (m_ShaderData[i] != New[i] &&
            New[i] != c_AnyNull)
        {
            m_ShaderData[i] = New[i];
            bDirty |= this->m_Bound[i] == nullptr;
        }
    }

    if (m_ShaderData.size() < rootSignatureBucketSize)
    {
        // Did we move to a larger bucket size? If so, fill the extra shader data to null (unknown) resource dimension
        m_ShaderData.resize(rootSignatureBucketSize, c_AnyNull);
        bDirty = true;
    }
    else if (m_ShaderData.size() > rootSignatureBucketSize)
    {
        // Did we move to a smaller bucket size? If so, shrink the shader data to fit
        // Don't need to mark as dirty since the root signature won't be able to address the stale descriptors
        m_ShaderData.resize(rootSignatureBucketSize);
    }

    if (!bDirty)
    {
        bDirty = DirtyBitsUpTo(static_cast<UINT>(rootSignatureBucketSize));
    }

    return bDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool CConstantBufferBoundState::UpdateBinding(_In_range_(0, NumBindings-1) UINT slot, _In_opt_ Resource* pBindable, EShaderStage stage) noexcept
{
    auto& Current = m_Bound[slot];
    if (pBindable)
    {
        m_NumBound = max(m_NumBound, slot + 1);
    }
    if (Current != pBindable)
    {
        ImmediateContext::ConstantBufferUnbound(Current, slot, stage);
        ImmediateContext::ConstantBufferBound(pBindable, slot, stage);
        Current = pBindable;

        // We skip calling TrimNumBound because we just use shader data to determine the actual amount to bind
        m_DirtyBits.set(slot);
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool CSamplerBoundState::UpdateBinding(_In_range_(0, NumBindings-1) UINT slot, _In_ Sampler* pBindable) noexcept
{
    auto& Current = m_Bound[slot];
    if (pBindable)
    {
        m_NumBound = max(m_NumBound, slot + 1);
    }
    if (Current != pBindable)
    {
        Current = pBindable;

        // We skip calling TrimNumBound because we just use shader data to determine the actual amount to bind
        m_DirtyBits.set(slot);
        return true;
    }
    return false;
}

inline ID3D12Resource* GetUnderlyingResource(Resource* pResource)
{
    if (!pResource)
        return nullptr;
    return pResource->GetUnderlyingResource();
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawInstanced(UINT countPerInstance, UINT instanceCount, UINT vertexStart, UINT instanceStart)
{
    try
    {
        PreDraw(); // throw ( _com_error )
        GetGraphicsCommandList()->DrawInstanced(countPerInstance, instanceCount, vertexStart, instanceStart);
        PostDraw();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::Draw(UINT count, UINT vertexStart)
{
    DrawInstanced(count, 1, vertexStart, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawIndexed(UINT indexCount, UINT indexStart, INT vertexStart)
{
    DrawIndexedInstanced(indexCount, 1, indexStart, vertexStart, 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawIndexedInstanced(UINT countPerInstance, UINT instanceCount, UINT indexStart, INT vertexStart, UINT instanceStart)
{
    try
    {
        PreDraw(); // throw ( _com_error )
        GetGraphicsCommandList()->DrawIndexedInstanced(countPerInstance, instanceCount, indexStart, vertexStart, instanceStart);
        PostDraw();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawAuto()
{
    // If there is no VB bound, then skip
    Resource* pVertexBuffer = m_CurrentState.m_VBs.GetBound()[0];
    if (!pVertexBuffer)
    {
        return;
    }

    try
    {
#ifdef USE_PIX
        PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"DrawAuto");
#endif
        EnsureDrawAutoResources(); // throw( _com_error )

        EnsureExecuteIndirectResources(); // throw( _com_error )

        // Transition the vertex buffer to the UAV state
        m_ResourceStateManager.TransitionResource(pVertexBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_ResourceStateManager.ApplyAllResourceTransitions();

        UINT VBOffset = m_auVertexOffsets[0] + GetDynamicBufferOffset(pVertexBuffer);
        UINT VBStride = m_auVertexStrides[0];

        UINT Constants[] =
        {
            VBOffset,
            VBStride
        };

        assert(0 == (pVertexBuffer->GetOffsetToStreamOutputSuffix() % sizeof(UINT)));
        static_assert(0 == (sizeof(SStreamOutputSuffix) % sizeof(UINT)), "Suffix must be UINT-aligned");

        FormatBuffer(
            pVertexBuffer->GetUnderlyingResource(),
            m_pDrawAutoPSO.get(),
            pVertexBuffer->GetOffsetToStreamOutputSuffix() / sizeof(UINT),
            sizeof(SStreamOutputSuffix) / sizeof(UINT),
            Constants
            ); // throw( _com_error )

        // This will transition the vertex buffer back to the correct state (DEFAULT_READ | INDIRECT_ARGUMENT)
        PreDraw(); // throw ( _com_error )
        GetGraphicsCommandList()->ExecuteIndirect(
            m_pDrawInstancedCommandSignature.get(),
            1,
            pVertexBuffer->GetUnderlyingResource(),
            pVertexBuffer->GetOffsetToStreamOutputSuffix() + offsetof(SStreamOutputSuffix, VertexCountPerInstance),
            nullptr,
            0
            );
        PostDraw();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawIndexedInstancedIndirect(Resource* pBuffer, UINT offset)
{
    m_ResourceStateManager.TransitionResource(pBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    try
    {
        EnsureExecuteIndirectResources(); // throw( _com_error )

        PreDraw(); // throw ( _com_error )
        auto pAPIBuffer = GetUnderlyingResource(pBuffer);

        GetGraphicsCommandList()->ExecuteIndirect(
            m_pDrawIndexedInstancedCommandSignature.get(),
            1,
            pAPIBuffer,
            offset + GetDynamicBufferOffset(pBuffer),
            nullptr,
            0
            );
        PostDraw();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DrawInstancedIndirect(Resource* pBuffer, UINT offset)
{
    m_ResourceStateManager.TransitionResource(pBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    
    try
    {
        EnsureExecuteIndirectResources(); // throw( _com_error )

        PreDraw(); // throw ( _com_error )
        auto pAPIBuffer = GetUnderlyingResource(pBuffer);

        GetGraphicsCommandList()->ExecuteIndirect(
            m_pDrawInstancedCommandSignature.get(),
            1,
            pAPIBuffer,
            offset + GetDynamicBufferOffset(pBuffer),
            nullptr,
            0
            );
        PostDraw();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::InsertUAVBarriersIfNeeded(CViewBoundState<UAV, D3D11_1_UAV_SLOT_COUNT>& UAVBindings, UINT NumUAVs) noexcept
{
    // Insert UAV barriers if necessary, and indicate UAV barriers will be necessary next time
    // TODO: Optimizations here could avoid inserting barriers on read-after-read
    auto pUAVs = UAVBindings.GetBound();
    m_vUAVBarriers.clear();
    for (UINT i = 0; i < NumUAVs; ++i)
    {
        if (pUAVs[i])
        {
            if (pUAVs[i]->m_pResource->m_Identity->m_LastUAVAccess == GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS))
            {
                m_vUAVBarriers.push_back({ D3D12_RESOURCE_BARRIER_TYPE_UAV });
                m_vUAVBarriers.back().UAV.pResource = pUAVs[i]->m_pResource->GetUnderlyingResource();
            }
            pUAVs[i]->m_pResource->m_Identity->m_LastUAVAccess = GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS);
        }
    }
    if (m_vUAVBarriers.size())
    {
        GetGraphicsCommandList()->ResourceBarrier((UINT)m_vUAVBarriers.size(), m_vUAVBarriers.data());
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::PreDraw() noexcept(false)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"PreDraw");
#endif
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    if (m_bUseRingBufferDescriptorHeaps)
    {
        // Always dirty the state when using ring buffer heaps because we can't safely reuse tables in that case.
        m_DirtyStates |= EDirtyBits::e_HeapBindingsDirty;
    }

    if (m_DirtyStates & e_GraphicsRootSignatureDirty)
    {
        m_StatesToReassert |= e_GraphicsBindingsDirty; // All bindings need to be reapplied
        if (m_CurrentState.m_pLastGraphicsRootSig == nullptr)
        {
            // We don't know, so we have to be conservative.
            m_DirtyStates |= e_GraphicsBindingsDirty;
        }
        else if (m_CurrentState.m_pLastGraphicsRootSig != m_CurrentState.m_pPSO->GetRootSignature())
        {
            RootSignatureDesc const& OldDesc = m_CurrentState.m_pLastGraphicsRootSig->m_Desc;
            RootSignatureDesc const& NewDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;
            static constexpr UINT64 CBDirtyBits[] = { e_PSConstantBuffersDirty, e_VSConstantBuffersDirty, e_GSConstantBuffersDirty, e_HSConstantBuffersDirty, e_DSConstantBuffersDirty };
            static constexpr UINT64 SRVDirtyBits[] = { e_PSShaderResourcesDirty, e_VSShaderResourcesDirty, e_GSShaderResourcesDirty, e_HSShaderResourcesDirty, e_DSShaderResourcesDirty };
            static constexpr UINT64 SamplerDirtyBits[] = { e_PSSamplersDirty, e_VSSamplersDirty, e_GSSamplersDirty, e_HSSamplersDirty, e_DSSamplersDirty };
            for (UINT i = 0; i < std::extent<decltype(OldDesc.m_ShaderStages)>::value; ++i)
            {
                if (NewDesc.m_ShaderStages[i].GetCBBindingCount() > OldDesc.m_ShaderStages[i].GetCBBindingCount())
                {
                    m_DirtyStates |= CBDirtyBits[i];
                }
                if (NewDesc.m_ShaderStages[i].GetSRVBindingCount() > OldDesc.m_ShaderStages[i].GetSRVBindingCount())
                {
                    m_DirtyStates |= SRVDirtyBits[i];
                }
                if (NewDesc.m_ShaderStages[i].GetSamplerBindingCount() > OldDesc.m_ShaderStages[i].GetSamplerBindingCount())
                {
                    m_DirtyStates |= SamplerDirtyBits[i];
                }
            }
            if (NewDesc.GetUAVBindingCount() > OldDesc.GetUAVBindingCount())
            {
                m_DirtyStates |= e_UnorderedAccessViewsDirty;
            }
        }
        m_CurrentState.m_pLastGraphicsRootSig = m_CurrentState.m_pPSO->GetRootSignature();
    }

    auto& RootSigDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;

    // Shader-declared bindings do not set pipeline dirty bits at bind time, only slot dirty bits
    // These slot dirty bits are only interesting if they are below the maximum shader-declared slot
    // Translate slot dirty bits to pipeline dirty bits now, since we know the shader declarations
    auto pfnSetDirtySRVBindings = [this](SStageState& Stage, SShaderDecls* pShader, const RootSignatureDesc::ShaderStage& shaderStage, EDirtyBits eBit)
    {
        const TDeclVector EmptyDecls;
        if (Stage.m_SRVs.IsDirty(pShader ? pShader->m_ResourceDecls : EmptyDecls, shaderStage.GetSRVBindingCount(), !!(m_DirtyStates & eBit))) { m_DirtyStates |= eBit; }
    };
    auto pfnSetDirtyCBBindings = [this](SStageState& Stage, const RootSignatureDesc::ShaderStage& shaderStage, EDirtyBits eBit)
    {
        if (Stage.m_CBs.IsDirty(shaderStage.GetCBBindingCount())) { m_DirtyStates |= eBit; }
    };
    auto pfnSetDirtySamplerBindings = [this](SStageState& Stage, const RootSignatureDesc::ShaderStage& shaderStage, EDirtyBits eBit)
    {
        if (Stage.m_Samplers.IsDirty(shaderStage.GetSamplerBindingCount())) { m_DirtyStates |= eBit; }
    };
    pfnSetDirtySRVBindings(m_CurrentState.m_PS, m_CurrentState.m_pPSO->GetShader<e_PS>(), RootSigDesc.GetShaderStage<e_PS>(), e_PSShaderResourcesDirty);
    pfnSetDirtySRVBindings(m_CurrentState.m_VS, m_CurrentState.m_pPSO->GetShader<e_VS>(), RootSigDesc.GetShaderStage<e_VS>(), e_VSShaderResourcesDirty);
    pfnSetDirtySRVBindings(m_CurrentState.m_GS, m_CurrentState.m_pPSO->GetShader<e_GS>(), RootSigDesc.GetShaderStage<e_GS>(), e_GSShaderResourcesDirty);
    pfnSetDirtySRVBindings(m_CurrentState.m_HS, m_CurrentState.m_pPSO->GetShader<e_HS>(), RootSigDesc.GetShaderStage<e_HS>(), e_HSShaderResourcesDirty);
    pfnSetDirtySRVBindings(m_CurrentState.m_DS, m_CurrentState.m_pPSO->GetShader<e_DS>(), RootSigDesc.GetShaderStage<e_DS>(), e_DSShaderResourcesDirty);

    pfnSetDirtyCBBindings(m_CurrentState.m_PS, RootSigDesc.GetShaderStage<e_PS>(), e_PSConstantBuffersDirty);
    pfnSetDirtyCBBindings(m_CurrentState.m_VS, RootSigDesc.GetShaderStage<e_VS>(), e_VSConstantBuffersDirty);
    pfnSetDirtyCBBindings(m_CurrentState.m_GS, RootSigDesc.GetShaderStage<e_GS>(), e_GSConstantBuffersDirty);
    pfnSetDirtyCBBindings(m_CurrentState.m_HS, RootSigDesc.GetShaderStage<e_HS>(), e_HSConstantBuffersDirty);
    pfnSetDirtyCBBindings(m_CurrentState.m_DS, RootSigDesc.GetShaderStage<e_DS>(), e_DSConstantBuffersDirty);

    pfnSetDirtySamplerBindings(m_CurrentState.m_PS, RootSigDesc.GetShaderStage<e_PS>(), e_PSSamplersDirty);
    pfnSetDirtySamplerBindings(m_CurrentState.m_VS, RootSigDesc.GetShaderStage<e_VS>(), e_VSSamplersDirty);
    pfnSetDirtySamplerBindings(m_CurrentState.m_GS, RootSigDesc.GetShaderStage<e_GS>(), e_GSSamplersDirty);
    pfnSetDirtySamplerBindings(m_CurrentState.m_HS, RootSigDesc.GetShaderStage<e_HS>(), e_HSSamplersDirty);
    pfnSetDirtySamplerBindings(m_CurrentState.m_DS, RootSigDesc.GetShaderStage<e_DS>(), e_DSSamplersDirty);

    // Note: UAV decl scratch memory is reserved upfront, so these operations will not throw.
    m_UAVDeclScratch.clear();
    auto pfnMergeUAVDecls = [this](SShaderDecls* pShader)
    {
        if (pShader)
        {
            for (UINT i = 0; i < pShader->m_UAVDecls.size(); ++i)
            {
                if (i >= m_UAVDeclScratch.size())
                {
                    m_UAVDeclScratch.insert(m_UAVDeclScratch.end(), pShader->m_UAVDecls.begin() + i, pShader->m_UAVDecls.end());
                    return;
                }
                else if (m_UAVDeclScratch[i] == RESOURCE_DIMENSION::UNKNOWN)
                {
                    m_UAVDeclScratch[i] = pShader->m_UAVDecls[i];
                }
            }
        }
    };
    pfnMergeUAVDecls(m_CurrentState.m_pPSO->GetShader<e_PS>());
    pfnMergeUAVDecls(m_CurrentState.m_pPSO->GetShader<e_VS>());
    pfnMergeUAVDecls(m_CurrentState.m_pPSO->GetShader<e_GS>());
    pfnMergeUAVDecls(m_CurrentState.m_pPSO->GetShader<e_HS>());
    pfnMergeUAVDecls(m_CurrentState.m_pPSO->GetShader<e_DS>());
    if (m_CurrentState.m_UAVs.IsDirty(m_UAVDeclScratch, RootSigDesc.GetUAVBindingCount(), !!(m_DirtyStates & e_UnorderedAccessViewsDirty)))
    {
        m_DirtyStates |= e_UnorderedAccessViewsDirty;
    }

    // Now that pipeline dirty bits are set appropriately, check if we need to update the descriptor heap
    UINT ViewHeapSlot = ReserveSlotsForBindings(m_ViewHeap, &ImmediateContext::CalculateViewSlotsForBindings<false>); // throw( _com_error )
    UINT SamplerHeapSlot = ReserveSlotsForBindings(m_SamplerHeap, &ImmediateContext::CalculateSamplerSlotsForBindings<false>); // throw( _com_error )

    auto& UAVBindings = m_CurrentState.m_UAVs;
    UINT numUAVs = RootSigDesc.GetUAVBindingCount();
    InsertUAVBarriersIfNeeded(UAVBindings, numUAVs);

    // Update the descriptor heap, and apply dirty bindings
    if (m_DirtyStates & e_GraphicsBindingsDirty)
    {
        // SRVs
        DirtyShaderResourcesHelper<e_PS>(ViewHeapSlot);
        DirtyShaderResourcesHelper<e_VS>(ViewHeapSlot);
        DirtyShaderResourcesHelper<e_GS>(ViewHeapSlot);
        DirtyShaderResourcesHelper<e_HS>(ViewHeapSlot);
        DirtyShaderResourcesHelper<e_DS>(ViewHeapSlot);

        // CBs
        DirtyConstantBuffersHelper<e_PS>(ViewHeapSlot);
        DirtyConstantBuffersHelper<e_VS>(ViewHeapSlot);
        DirtyConstantBuffersHelper<e_GS>(ViewHeapSlot);
        DirtyConstantBuffersHelper<e_HS>(ViewHeapSlot);
        DirtyConstantBuffersHelper<e_DS>(ViewHeapSlot);

        // Samplers
        DirtySamplersHelper<e_PS>(SamplerHeapSlot);
        DirtySamplersHelper<e_VS>(SamplerHeapSlot);
        DirtySamplersHelper<e_GS>(SamplerHeapSlot);
        DirtySamplersHelper<e_HS>(SamplerHeapSlot);
        DirtySamplersHelper<e_DS>(SamplerHeapSlot);

        if (m_DirtyStates & e_UnorderedAccessViewsDirty)
        {
            auto& UAVTableBase = m_CurrentState.m_UAVTableBase;
            static const UINT MaxUAVs = UAVBindings.NumBindings;
            assert(ViewHeapSlot + numUAVs <= m_ViewHeap.m_Desc.NumDescriptors);

            D3D12_CPU_DESCRIPTOR_HANDLE UAVDescriptors[MaxUAVs];
            UAVBindings.FillDescriptors(UAVDescriptors, m_NullUAVs, RootSigDesc.GetUAVBindingCount());

            UAVTableBase = m_ViewHeap.GPUHandle(ViewHeapSlot);
            D3D12_CPU_DESCRIPTOR_HANDLE UAVTableBaseCPU = m_ViewHeap.CPUHandle(ViewHeapSlot);

            m_pDevice12->CopyDescriptors(1, &UAVTableBaseCPU, &numUAVs,
                numUAVs, UAVDescriptors, nullptr /*sizes*/,
                m_ViewHeap.m_Desc.Type);

            ViewHeapSlot += numUAVs;
        }
    }

    // Now the current state is up to date, let's apply it to the command list
    m_StatesToReassert |= (m_DirtyStates & e_GraphicsStateDirty);

    if (m_StatesToReassert & e_FirstDraw)
    {
        m_CurrentState.m_PS.m_SRVs.ReassertResourceState();
        m_CurrentState.m_VS.m_SRVs.ReassertResourceState();
        m_CurrentState.m_GS.m_SRVs.ReassertResourceState();
        m_CurrentState.m_HS.m_SRVs.ReassertResourceState();
        m_CurrentState.m_DS.m_SRVs.ReassertResourceState();

        m_CurrentState.m_PS.m_CBs.ReassertResourceState();
        m_CurrentState.m_VS.m_CBs.ReassertResourceState();
        m_CurrentState.m_GS.m_CBs.ReassertResourceState();
        m_CurrentState.m_HS.m_CBs.ReassertResourceState();
        m_CurrentState.m_DS.m_CBs.ReassertResourceState();

        m_CurrentState.m_UAVs.ReassertResourceState();
        m_CurrentState.m_VBs.ReassertResourceState();
        m_CurrentState.m_IB.ReassertResourceState();
        m_CurrentState.m_RTVs.ReassertResourceState();
        m_CurrentState.m_DSVs.ReassertResourceState();
        m_CurrentState.m_SO.ReassertResourceState();
    }

    m_ResourceStateManager.ApplyAllResourceTransitions(true);

    if (m_StatesToReassert & e_GraphicsStateDirty)
    {
        if (m_StatesToReassert & e_GraphicsRootSignatureDirty)
        {
            GetGraphicsCommandList()->SetGraphicsRootSignature(m_CurrentState.m_pPSO->GetRootSignature()->GetForImmediateUse());
            m_StatesToReassert |= e_GraphicsBindingsDirty;
        }

        if (m_StatesToReassert & e_PipelineStateDirty)
        {
            auto pPSO = m_CurrentState.m_pPSO->GetForUse(COMMAND_LIST_TYPE::GRAPHICS);
            if (!pPSO)
            {
                throw _com_error(S_OK);
            }
            GetGraphicsCommandList()->SetPipelineState(pPSO);
        }

        RefreshNonHeapBindings(m_StatesToReassert);

        // SRVs
        ApplyShaderResourcesHelper<e_PS>();
        ApplyShaderResourcesHelper<e_VS>();
        ApplyShaderResourcesHelper<e_GS>();
        ApplyShaderResourcesHelper<e_HS>();
        ApplyShaderResourcesHelper<e_DS>();

        // CBs
        ApplyConstantBuffersHelper<e_PS>();
        ApplyConstantBuffersHelper<e_VS>();
        ApplyConstantBuffersHelper<e_GS>();
        ApplyConstantBuffersHelper<e_HS>();
        ApplyConstantBuffersHelper<e_DS>();

        // Samplers
        ApplySamplersHelper<e_PS>();
        ApplySamplersHelper<e_VS>();
        ApplySamplersHelper<e_GS>();
        ApplySamplersHelper<e_HS>();
        ApplySamplersHelper<e_DS>();

        if (m_StatesToReassert & e_UnorderedAccessViewsDirty)
        {
            static const UINT UAVTableIndex = 15;
            auto const& UAVTableBase = m_CurrentState.m_UAVTableBase;

            GetGraphicsCommandList()->SetGraphicsRootDescriptorTable(UAVTableIndex, UAVTableBase);
        }

        // States that cannot be dirty (no recomputing necessary)
        if (m_StatesToReassert & e_PrimitiveTopologyDirty)
        {
            GetGraphicsCommandList()->IASetPrimitiveTopology(m_PrimitiveTopology);
        }

        if (m_StatesToReassert & e_BlendFactorDirty)
        {
            GetGraphicsCommandList()->OMSetBlendFactor(m_BlendFactor);
        }

        if (m_StatesToReassert & e_StencilRefDirty)
        {
            GetGraphicsCommandList()->OMSetStencilRef(m_uStencilRef);
        }

        if (m_StatesToReassert & e_ViewportsDirty)
        {
            GetGraphicsCommandList()->RSSetViewports(m_uNumViewports, reinterpret_cast<const D3D12_VIEWPORT*>(m_aViewports));
        }

        if (m_StatesToReassert & e_ScissorRectsDirty)
        {
            SetScissorRectsHelper();
        }
    }

    m_StatesToReassert &= ~e_GraphicsStateDirty;
    m_DirtyStates &= ~e_GraphicsStateDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <bool bDispatch>
inline UINT ImmediateContext::CalculateViewSlotsForBindings() noexcept
{
    UINT NumRequiredSlots = 0;
    auto pfnAccumulate = [this, &NumRequiredSlots](UINT dirtyBit, UINT count)
    {
        if (m_DirtyStates & dirtyBit) { NumRequiredSlots += count; }
    };
    auto& RootSigDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;
    if (bDispatch)
    {
        pfnAccumulate(e_CSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_CS>().GetSRVBindingCount());
        pfnAccumulate(e_CSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_CS>().GetCBBindingCount());
        pfnAccumulate(e_CSUnorderedAccessViewsDirty, RootSigDesc.GetUAVBindingCount());
    }
    else
    {
        pfnAccumulate(e_PSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_PS>().GetSRVBindingCount());
        pfnAccumulate(e_VSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_VS>().GetSRVBindingCount());
        pfnAccumulate(e_GSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_GS>().GetSRVBindingCount());
        pfnAccumulate(e_HSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_HS>().GetSRVBindingCount());
        pfnAccumulate(e_DSShaderResourcesDirty, RootSigDesc.GetShaderStage<e_DS>().GetSRVBindingCount());

        pfnAccumulate(e_PSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_PS>().GetCBBindingCount());
        pfnAccumulate(e_VSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_VS>().GetCBBindingCount());
        pfnAccumulate(e_GSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_GS>().GetCBBindingCount());
        pfnAccumulate(e_HSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_HS>().GetCBBindingCount());
        pfnAccumulate(e_DSConstantBuffersDirty, RootSigDesc.GetShaderStage<e_DS>().GetCBBindingCount());

        pfnAccumulate(e_UnorderedAccessViewsDirty, RootSigDesc.GetUAVBindingCount());
    }
    return NumRequiredSlots;
}

template <bool bDispatch>
inline UINT ImmediateContext::CalculateSamplerSlotsForBindings() noexcept
{
    UINT NumRequiredSlots = 0;
    auto pfnAccumulate = [this, &NumRequiredSlots](UINT dirtyBit, UINT count)
    {
        if (m_DirtyStates & dirtyBit) { NumRequiredSlots += count; }
    };
    auto& RootSigDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;
    if (bDispatch)
    {
        pfnAccumulate(e_CSSamplersDirty, RootSigDesc.GetShaderStage<e_CS>().GetSamplerBindingCount());
    }
    else
    {
        pfnAccumulate(e_PSSamplersDirty, RootSigDesc.GetShaderStage<e_PS>().GetSamplerBindingCount());
        pfnAccumulate(e_VSSamplersDirty, RootSigDesc.GetShaderStage<e_VS>().GetSamplerBindingCount());
        pfnAccumulate(e_GSSamplersDirty, RootSigDesc.GetShaderStage<e_GS>().GetSamplerBindingCount());
        pfnAccumulate(e_HSSamplersDirty, RootSigDesc.GetShaderStage<e_HS>().GetSamplerBindingCount());
        pfnAccumulate(e_DSSamplersDirty, RootSigDesc.GetShaderStage<e_DS>().GetSamplerBindingCount());
    }
    return NumRequiredSlots;
}


//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader>
inline void ImmediateContext::DirtyShaderResourcesHelper(UINT& HeapSlot) noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    if ((m_DirtyStates & TShaderTraits::c_ShaderResourcesDirty) == 0)
    {
        return;
    }

    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    auto& SRVBindings = CurrentState.m_SRVs;
    UINT RootSigHWM = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc.GetShaderStage<eShader>().GetSRVBindingCount();
    UINT numSRVs = RootSigHWM;
    static const UINT MaxSRVs = SRVBindings.NumBindings;
    assert(HeapSlot + numSRVs <= m_ViewHeap.m_Desc.NumDescriptors);

    D3D12_CPU_DESCRIPTOR_HANDLE Descriptors[MaxSRVs];
    SRVBindings.FillDescriptors(Descriptors, m_NullSRVs, RootSigHWM);

    CurrentState.m_SRVTableBase = m_ViewHeap.GPUHandle(HeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE SRVTableBaseCPU = m_ViewHeap.CPUHandle(HeapSlot);

    m_pDevice12->CopyDescriptors(1, &SRVTableBaseCPU, &numSRVs,
        numSRVs, Descriptors, nullptr /*sizes*/,
        m_ViewHeap.m_Desc.Type);

    HeapSlot += numSRVs;
}

//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader>
inline void ImmediateContext::DirtyConstantBuffersHelper(UINT& HeapSlot) noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    if ((m_DirtyStates & TShaderTraits::c_ConstantBuffersDirty) == 0)
    {
        return;
    }

    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    auto& CBBindings = CurrentState.m_CBs;
    UINT numCBs = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc.GetShaderStage<eShader>().GetCBBindingCount();
    static const UINT MaxCBs = CBBindings.NumBindings;

    assert(HeapSlot + numCBs <= m_ViewHeap.m_Desc.NumDescriptors);

    for (UINT i = 0; i < numCBs; ++i)
    {
        CBBindings.ResetDirty(i);
        auto pBuffer = CBBindings.GetBound()[i];
        D3D12_CONSTANT_BUFFER_VIEW_DESC CBDesc;
        UINT APIOffset = CurrentState.m_uConstantBufferOffsets[i] * 16;
        UINT APISize = CurrentState.m_uConstantBufferCounts[i] * 16;
        GetBufferViewDesc(pBuffer, CBDesc, APIOffset, APISize);

        D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = m_ViewHeap.CPUHandle(HeapSlot + i);
        m_pDevice12->CreateConstantBufferView(&CBDesc, Descriptor);
    }

    CurrentState.m_CBTableBase = m_ViewHeap.GPUHandle(HeapSlot);

    HeapSlot += numCBs;
}

//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader>
inline void ImmediateContext::DirtySamplersHelper(UINT& HeapSlot) noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    if ((m_DirtyStates & TShaderTraits::c_SamplersDirty) == 0)
    {
        return;
    }

    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    UINT RootSigHWM = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc.GetShaderStage<eShader>().GetSamplerBindingCount();
    UINT numSamplers = RootSigHWM;
    auto& SamplerBindings = CurrentState.m_Samplers;
    static const UINT MaxSamplers = SamplerBindings.NumBindings;

    assert(HeapSlot + numSamplers <= m_SamplerHeap.m_Desc.NumDescriptors);

    D3D12_CPU_DESCRIPTOR_HANDLE Descriptors[MaxSamplers];
    SamplerBindings.FillDescriptors(Descriptors, &m_NullSampler, RootSigHWM);

    CurrentState.m_SamplerTableBase = m_SamplerHeap.GPUHandle(HeapSlot);
    D3D12_CPU_DESCRIPTOR_HANDLE SamplerTableBaseCPU = m_SamplerHeap.CPUHandle(HeapSlot);

    m_pDevice12->CopyDescriptors(1, &SamplerTableBaseCPU, &numSamplers,
        numSamplers, Descriptors, nullptr /*sizes*/,
        m_SamplerHeap.m_Desc.Type);

    HeapSlot += numSamplers;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TDesc>
inline void GetBufferViewDesc(Resource* pBuffer, TDesc& Desc, UINT APIOffset, UINT APISize = -1)
{
    if (pBuffer)
    {
        Desc.SizeInBytes =
            min(GetDynamicBufferSize<TDesc>(pBuffer, APIOffset), APISize);
        Desc.BufferLocation = Desc.SizeInBytes == 0 ? 0 :
            // TODO: Cache the GPU VA, frequent calls to this cause a CPU hotspot
            (pBuffer->GetUnderlyingResource()->GetGPUVirtualAddress() // Base of the DX12 resource
                + pBuffer->GetSubresourcePlacement(0).Offset // Base of the DX11 resource after renaming
                + APIOffset); // Offset from the base of the DX11 resource
    }
    else
    {
        Desc.BufferLocation = 0;
        Desc.SizeInBytes = 0;
    }
}

template<EShaderStage eShader> struct SRVBindIndices;
template<> struct SRVBindIndices<e_PS> { static const UINT c_TableIndex = 1; };
template<> struct SRVBindIndices<e_VS> { static const UINT c_TableIndex = 4; };
template<> struct SRVBindIndices<e_GS> { static const UINT c_TableIndex = 7; };
template<> struct SRVBindIndices<e_HS> { static const UINT c_TableIndex = 10; };
template<> struct SRVBindIndices<e_DS> { static const UINT c_TableIndex = 13; };
template<> struct SRVBindIndices<e_CS> { static const UINT c_TableIndex = 1; };

template<EShaderStage eShader>
inline void ImmediateContext::ApplyShaderResourcesHelper() noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    if ((m_StatesToReassert & TShaderTraits::c_ShaderResourcesDirty) == 0)
    {
        return;
    }

    (GetGraphicsCommandList()->*DescriptorBindFuncs<eShader>::GetBindFunc())(
        SRVBindIndices<eShader>::c_TableIndex,
        CurrentState.m_SRVTableBase);
}

//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader> struct CBBindIndices;
template<> struct CBBindIndices<e_PS> { static const UINT c_TableIndex = 0; };
template<> struct CBBindIndices<e_VS> { static const UINT c_TableIndex = 3; };
template<> struct CBBindIndices<e_GS> { static const UINT c_TableIndex = 6; };
template<> struct CBBindIndices<e_HS> { static const UINT c_TableIndex = 9; };
template<> struct CBBindIndices<e_DS> { static const UINT c_TableIndex = 12; };
template<> struct CBBindIndices<e_CS> { static const UINT c_TableIndex = 0; };

template<EShaderStage eShader>
inline void ImmediateContext::ApplyConstantBuffersHelper() noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    if ((m_StatesToReassert & TShaderTraits::c_ConstantBuffersDirty) == 0)
    {
        return;
    }

    (GetGraphicsCommandList()->*DescriptorBindFuncs<eShader>::GetBindFunc())(
        CBBindIndices<eShader>::c_TableIndex,
        CurrentState.m_CBTableBase);
}

//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader> struct SamplerBindIndices;
template<> struct SamplerBindIndices<e_PS> { static const UINT c_TableIndex = 2; };
template<> struct SamplerBindIndices<e_VS> { static const UINT c_TableIndex = 5; };
template<> struct SamplerBindIndices<e_GS> { static const UINT c_TableIndex = 8; };
template<> struct SamplerBindIndices<e_HS> { static const UINT c_TableIndex = 11; };
template<> struct SamplerBindIndices<e_DS> { static const UINT c_TableIndex = 14; };
template<> struct SamplerBindIndices<e_CS> { static const UINT c_TableIndex = 2; };

template<EShaderStage eShader>
inline void ImmediateContext::ApplySamplersHelper() noexcept
{
    typedef SShaderTraits<eShader> TShaderTraits;
    SStageState& CurrentState = TShaderTraits::CurrentStageState(m_CurrentState);
    if ((m_StatesToReassert & TShaderTraits::c_SamplersDirty) == 0)
    {
        return;
    }

    (GetGraphicsCommandList()->*DescriptorBindFuncs<eShader>::GetBindFunc())(
        SamplerBindIndices<eShader>::c_TableIndex,
        CurrentState.m_SamplerTableBase);
}

//----------------------------------------------------------------------------------------------------------------------------------
template<EShaderStage eShader> struct DescriptorBindFuncs
{
    static decltype(&ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable) GetBindFunc()
    {
        return &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable;
    }
};
template<> struct DescriptorBindFuncs<e_CS>
{
    static decltype(&ID3D12GraphicsCommandList::SetComputeRootDescriptorTable) GetBindFunc()
    {
        return &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable;
    }
};

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::Dispatch(UINT x, UINT y, UINT z)
{
    // Early out if no compute shader has been set
    if (!m_CurrentState.m_pPSO->GetComputeDesc().CS.pShaderBytecode)
    {
        return;
    }

    try
    {
        PreDispatch(); // throw ( _com_error )
        GetGraphicsCommandList()->Dispatch(x, y, z);
        PostDispatch();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void TRANSLATION_API ImmediateContext::DispatchIndirect(Resource* pBuffer, UINT offset)
{
    // Early out if no compute shader has been set
    if (!m_CurrentState.m_pPSO->GetComputeDesc().CS.pShaderBytecode)
    {
        return;
    }

    m_ResourceStateManager.TransitionResource(pBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    
    try
    {
        EnsureExecuteIndirectResources(); // throw( _com_error )

        PreDispatch(); // throw ( _com_error )
        auto pAPIBuffer = GetUnderlyingResource(pBuffer);

        GetGraphicsCommandList()->ExecuteIndirect(
            m_pDispatchCommandSignature.get(),
            1,
            pAPIBuffer,
            offset + GetDynamicBufferOffset(pBuffer),
            nullptr,
            0
            );
        PostDispatch();
    }
    catch (_com_error) {} // already handled, but can't touch the command list
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::PreDispatch() noexcept(false)
{
#ifdef USE_PIX
    PIXScopedEvent(GetGraphicsCommandList(), 0ull, L"PreDispatch");
#endif
    PreRender(COMMAND_LIST_TYPE::GRAPHICS);

    if (m_bUseRingBufferDescriptorHeaps)
    {
        // Always dirty the state when using ring buffer heaps because we can't safely reuse tables in that case.
        m_DirtyStates |= EDirtyBits::e_HeapBindingsDirty;
    }

    if (m_DirtyStates & e_ComputeRootSignatureDirty)
    {
        m_StatesToReassert |= e_ComputeBindingsDirty; // All bindings need to be reapplied
        if (m_CurrentState.m_pLastComputeRootSig == nullptr)
        {
            // We don't know, so we have to be conservative.
            m_DirtyStates |= e_ComputeBindingsDirty;
        }
        else if (m_CurrentState.m_pLastComputeRootSig != m_CurrentState.m_pPSO->GetRootSignature())
        {
            RootSignatureDesc const& OldDesc = m_CurrentState.m_pLastComputeRootSig->m_Desc;
            RootSignatureDesc const& NewDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;
            if (NewDesc.m_ShaderStages[0].GetCBBindingCount() > OldDesc.m_ShaderStages[0].GetCBBindingCount())
            {
                m_DirtyStates |= e_CSConstantBuffersDirty;
            }
            if (NewDesc.m_ShaderStages[0].GetSRVBindingCount() > OldDesc.m_ShaderStages[0].GetSRVBindingCount())
            {
                m_DirtyStates |= e_CSShaderResourcesDirty;
            }
            if (NewDesc.m_ShaderStages[0].GetSamplerBindingCount() > OldDesc.m_ShaderStages[0].GetSamplerBindingCount())
            {
                m_DirtyStates |= e_CSSamplersDirty;
            }
            if (NewDesc.GetUAVBindingCount() > OldDesc.GetUAVBindingCount())
            {
                m_DirtyStates |= e_CSUnorderedAccessViewsDirty;
            }
        }
        m_CurrentState.m_pLastComputeRootSig = m_CurrentState.m_pPSO->GetRootSignature();
    }

    // See PreDraw for comments regarding how dirty bits for bindings are managed
    auto& RootSigDesc = m_CurrentState.m_pPSO->GetRootSignature()->m_Desc;
    auto& shaderStage = RootSigDesc.GetShaderStage<e_CS>();

    const TDeclVector EmptyDecls;
    auto pComputeShader = m_CurrentState.m_pPSO->GetShader<e_CS>();
    m_DirtyStates |= m_CurrentState.m_CS.m_SRVs.IsDirty(pComputeShader ? pComputeShader->m_ResourceDecls : EmptyDecls, shaderStage.GetSRVBindingCount(), !!(m_DirtyStates & e_CSShaderResourcesDirty)) ? e_CSShaderResourcesDirty : 0;
    m_DirtyStates |= m_CurrentState.m_CS.m_CBs.IsDirty(shaderStage.GetCBBindingCount()) ? e_CSConstantBuffersDirty : 0;
    m_DirtyStates |= m_CurrentState.m_CS.m_Samplers.IsDirty(shaderStage.GetSamplerBindingCount()) ? e_CSSamplersDirty : 0;
    m_DirtyStates |= m_CurrentState.m_CSUAVs.IsDirty(pComputeShader ? pComputeShader->m_UAVDecls : EmptyDecls, RootSigDesc.GetUAVBindingCount(), !!(m_DirtyStates & e_CSUnorderedAccessViewsDirty)) ? e_CSUnorderedAccessViewsDirty : 0;

    // Now that pipeline dirty bits are set appropriately, check if we need to update the descriptor heap
    UINT ViewHeapSlot = ReserveSlotsForBindings(m_ViewHeap, &ImmediateContext::CalculateViewSlotsForBindings<true>); // throw( _com_error )
    UINT SamplerHeapSlot = 0;
    if (!ComputeOnly())
    {
        SamplerHeapSlot = ReserveSlotsForBindings(m_SamplerHeap, &ImmediateContext::CalculateSamplerSlotsForBindings<true>); // throw( _com_error )
    }

    auto& UAVBindings = m_CurrentState.m_CSUAVs;
    UINT numUAVs = RootSigDesc.GetUAVBindingCount();
    InsertUAVBarriersIfNeeded(UAVBindings, numUAVs);

    // Second pass copies data into the the descriptor heap
    if (m_DirtyStates & e_ComputeBindingsDirty)
    {
        DirtyShaderResourcesHelper<e_CS>(ViewHeapSlot);
        DirtyConstantBuffersHelper<e_CS>(ViewHeapSlot);
        if (!ComputeOnly())
        {
            DirtySamplersHelper<e_CS>(SamplerHeapSlot);
        }
        if (m_DirtyStates & e_CSUnorderedAccessViewsDirty)
        {
            auto& UAVTableBase = m_CurrentState.m_CSUAVTableBase;

            static const UINT MaxUAVs = UAVBindings.NumBindings;
            assert(ViewHeapSlot + numUAVs <= m_ViewHeap.m_Desc.NumDescriptors);

            D3D12_CPU_DESCRIPTOR_HANDLE UAVDescriptors[MaxUAVs];
            UAVBindings.FillDescriptors(UAVDescriptors, m_NullUAVs, RootSigDesc.GetUAVBindingCount());

            UAVTableBase = m_ViewHeap.GPUHandle(ViewHeapSlot);
            D3D12_CPU_DESCRIPTOR_HANDLE UAVTableBaseCPU = m_ViewHeap.CPUHandle(ViewHeapSlot);

            m_pDevice12->CopyDescriptors(1, &UAVTableBaseCPU, &numUAVs,
                numUAVs, UAVDescriptors, nullptr /*sizes*/,
                m_ViewHeap.m_Desc.Type);

            ViewHeapSlot += numUAVs;
        }
    }

    m_StatesToReassert |= (m_DirtyStates & e_ComputeStateDirty);

    if (m_StatesToReassert & e_FirstDispatch)
    {
        m_CurrentState.m_CS.m_SRVs.ReassertResourceState();
        m_CurrentState.m_CS.m_CBs.ReassertResourceState();
        m_CurrentState.m_CSUAVs.ReassertResourceState();
    }

    m_ResourceStateManager.ApplyAllResourceTransitions(true);

    if (m_StatesToReassert & e_ComputeStateDirty)
    {
        if (m_StatesToReassert & e_ComputeRootSignatureDirty)
        {
            GetGraphicsCommandList()->SetComputeRootSignature(m_CurrentState.m_pPSO->GetRootSignature()->GetForImmediateUse());
            m_StatesToReassert |= e_ComputeBindingsDirty;
        }

        if (m_StatesToReassert & e_PipelineStateDirty)
        {
            auto pPSO = m_CurrentState.m_pPSO->GetForUse(COMMAND_LIST_TYPE::GRAPHICS);
            if (!pPSO)
            {
                throw _com_error(S_OK);
            }
            GetGraphicsCommandList()->SetPipelineState(pPSO);
        }

        ApplyShaderResourcesHelper<e_CS>();
        ApplyConstantBuffersHelper<e_CS>();
        if (ComputeOnly())
        {
            if (m_StatesToReassert & e_CSSamplersDirty)
            {
                // For compute-only, we turn our sampler tables into SRVs.
                // Make sure we bind something that's valid to the sampler slot, and just make it mirror the SRVs.
                GetGraphicsCommandList()->SetComputeRootDescriptorTable(SamplerBindIndices<e_CS>::c_TableIndex, m_CurrentState.m_CS.m_SRVTableBase);
            }
        }
        else
        {
            ApplySamplersHelper<e_CS>();
        }
        if (m_StatesToReassert & e_CSUnorderedAccessViewsDirty)
        {
            static const UINT UAVTableIndex = 3;
            auto const& UAVTableBase = m_CurrentState.m_CSUAVTableBase;

            GetGraphicsCommandList()->SetComputeRootDescriptorTable(UAVTableIndex, UAVTableBase);
        }
    }

    m_StatesToReassert &= ~e_ComputeStateDirty;
    m_DirtyStates &= ~e_ComputeStateDirty;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline ID3D12CommandQueue *ImmediateContext::GetCommandQueue(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->GetCommandQueue();
    }
    else
    {
        return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline ID3D12GraphicsCommandList *ImmediateContext::GetGraphicsCommandList() noexcept
{
    return m_CommandLists[(UINT)COMMAND_LIST_TYPE::GRAPHICS]->GetGraphicsCommandList();
}

//----------------------------------------------------------------------------------------------------------------------------------
inline ID3D12VideoDecodeCommandList2 *ImmediateContext::GetVideoDecodeCommandList() noexcept
{
    if (m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_DECODE])
    {
        return  m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_DECODE]->GetVideoDecodeCommandList();
    }
    else
    {
        return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline ID3D12VideoProcessCommandList2 *ImmediateContext::GetVideoProcessCommandList() noexcept
{
    if (m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_PROCESS])
    {
        return  m_CommandLists[(UINT)COMMAND_LIST_TYPE::VIDEO_PROCESS]->GetVideoProcessCommandList();
    }
    else
    {
        return nullptr;
    }
}

// There is an MSVC bug causing a bogus warning to be emitted here for x64 only, while compiling ApplyAllResourceTransitions
#pragma warning(push)
#pragma warning(disable: 4789)
//----------------------------------------------------------------------------------------------------------------------------------
inline CommandListManager *ImmediateContext::GetCommandListManager(COMMAND_LIST_TYPE type) noexcept
{
    return type != COMMAND_LIST_TYPE::UNKNOWN ? m_CommandLists[(UINT)type].get() : nullptr;
}
#pragma warning(pop)

//----------------------------------------------------------------------------------------------------------------------------------
inline ID3D12CommandList *ImmediateContext::GetCommandList(COMMAND_LIST_TYPE commandListType) noexcept
{
    if (commandListType != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)commandListType])
    {
        return m_CommandLists[(UINT)commandListType]->GetCommandList();
    }
    else
    {
        return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT64 ImmediateContext::GetCommandListID(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->GetCommandListID();
    }
    else
    {
        return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT64 ImmediateContext::GetCommandListIDInterlockedRead(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->GetCommandListIDInterlockedRead();
    }
    else
    {
        return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT64 ImmediateContext::GetCommandListIDWithCommands(COMMAND_LIST_TYPE type) noexcept
{
    // This method gets the ID of the last command list that actually has commands, which is either
    // the current command list, if it has commands, or the previously submitted command list if the
    // current is empty.
    //
    // The result of this method is the fence id that will be signaled after a flush, and is used so that
    // Async::End can track query completion correctly.
    UINT64 Id = 0;
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        Id = m_CommandLists[(UINT)type]->GetCommandListID();
        assert(Id);
        if (!m_CommandLists[(UINT)type]->HasCommands() && !m_CommandLists[(UINT)type]->NeedSubmitFence())
        {
            Id -= 1; // Go back one command list
        }
    }
    return Id;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT64 ImmediateContext::GetCompletedFenceValue(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->GetCompletedFenceValue();
    }
    else
    {
        return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline Fence *ImmediateContext::GetFence(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->GetFence();
    }
    else
    {
        return nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::CloseCommandList(UINT commandListTypeMask) noexcept
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i])
        {
            m_CommandLists[i]->CloseCommandList();
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::ResetCommandList(UINT commandListTypeMask) noexcept
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i])
        {
            m_CommandLists[i]->ResetCommandList();
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------------------
inline HRESULT ImmediateContext::EnqueueSetEvent(UINT commandListTypeMask, HANDLE hEvent) noexcept
{
#ifdef USE_PIX
    PIXSetMarker(0ull, L"EnqueueSetEvent");
#endif
    HRESULT hr = S_OK;
    ID3D12Fence *pFences[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
    UINT64 FenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
    UINT nLists = 0;
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i])
        {
            pFences[nLists] = m_CommandLists[i]->GetFence()->Get();
            try {
                FenceValues[nLists] = m_CommandLists[i]->EnsureFlushedAndFenced(); // throws
            }
            catch (_com_error& e)
            {
                return e.Error();
            }
            catch (std::bad_alloc&)
            {
                return E_OUTOFMEMORY;
            }

            ++nLists;
        }
    }
    hr = m_pDevice12_1->SetEventOnMultipleFenceCompletion(
             pFences,
             FenceValues,
             nLists,
             D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL,
             hEvent);
    return hr;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline HRESULT ImmediateContext::EnqueueSetEvent(COMMAND_LIST_TYPE commandListType, HANDLE hEvent) noexcept
{
    if (commandListType != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)commandListType])
    {
        return m_CommandLists[(UINT)commandListType]->EnqueueSetEvent(hEvent);
    }
    else
    {
        return E_UNEXPECTED;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool ImmediateContext::WaitForCompletion(UINT commandListTypeMask) noexcept
{
    UINT nLists = 0;
    HANDLE hEvents[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i)) && m_CommandLists[i])
        {
            hEvents[nLists] = m_CommandLists[i]->GetEvent();
            if (FAILED(m_CommandLists[i]->EnqueueSetEvent(hEvents[nLists])))
            {
                return false;
            }
            ++nLists;
        }
    }
    DWORD waitRet = WaitForMultipleObjects(nLists, hEvents, TRUE, INFINITE);
    UNREFERENCED_PARAMETER(waitRet);
    assert(waitRet == WAIT_OBJECT_0);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool ImmediateContext::WaitForCompletion(COMMAND_LIST_TYPE commandListType)
{
    if (commandListType != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)commandListType])
    {
        return m_CommandLists[(UINT)commandListType]->WaitForCompletion(); // throws
    }
    else
    {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool ImmediateContext::WaitForFenceValue(COMMAND_LIST_TYPE commandListType, UINT64 FenceValue)
{
    if (commandListType != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)commandListType])
    {
        return m_CommandLists[(UINT)commandListType]->WaitForFenceValue(FenceValue); // throws
    }
    else
    {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::SubmitCommandList(UINT commandListTypeMask)
{
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if ((commandListTypeMask & (1 << i))  &&  m_CommandLists[i])
        {
            m_CommandLists[i]->SubmitCommandList(); // throws
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::SubmitCommandList(COMMAND_LIST_TYPE commandListType)
{
    if (commandListType != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)commandListType])
    {
        m_CommandLists[(UINT)commandListType]->SubmitCommandList(); // throws
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline void ImmediateContext::AdditionalCommandsAdded(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        m_CommandLists[(UINT)type]->AdditionalCommandsAdded();
    }
}

inline void ImmediateContext::UploadHeapSpaceAllocated(COMMAND_LIST_TYPE type, UINT64 HeapSize) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        m_CommandLists[(UINT)type]->UploadHeapSpaceAllocated(HeapSize);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline bool ImmediateContext::HasCommands(COMMAND_LIST_TYPE type) noexcept
{
    if (type != COMMAND_LIST_TYPE::UNKNOWN  &&  m_CommandLists[(UINT)type])
    {
        return m_CommandLists[(UINT)type]->HasCommands();
    }
    else
    {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
inline UINT ImmediateContext::GetCurrentCommandListTypeMask() noexcept
{
    UINT Mask = 0;
    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
    {
        if (m_CommandLists[i])
        {
            Mask |= (1 << i);
        }
    }
    return Mask;
}

inline UINT ImmediateContext::GetCommandListTypeMaskForQuery(EQueryType query) noexcept
{
    UINT commandListTypeMask = GetCurrentCommandListTypeMask();

    if (query != e_QUERY_EVENT &&
        query != e_QUERY_TIMESTAMP &&
        query != e_QUERY_TIMESTAMPDISJOINT)
    {
        commandListTypeMask &= COMMAND_LIST_TYPE_GRAPHICS_MASK;
    }
    return commandListTypeMask;
}


};
