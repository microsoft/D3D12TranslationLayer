// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{

//----------------------------------------------------------------------------------------------------------------------------------
// Ptr on input points to current command, and on output points to next command
template <typename TCmd> TCmd const& GetCommandData(const void*& pPtrToCommandValue)
{
    // Ensure this overall structure is aligned to return pointer to next structure. Internal alignment handled by the compiler.
    struct alignas(BatchedContext::BatchPrimitive)Temp { UINT CommandValue; TCmd Command; };

    Temp const* pPtr = reinterpret_cast<Temp const*>(pPtrToCommandValue);
    pPtrToCommandValue = pPtr + 1;
    return pPtr->Command;
}

// Ptr on input points to current command, and on output points to next command
template <typename TCmd, typename TEntryCountType, typename TEntry>
TCmd const& GetCommandDataVariableSize(const void*& pPtrToCommandValue, UINT TEntryCountType::*NumEntries, TEntry const*& entries)
{
    // Compiler ensures that pointer to first entry is aligned correctly
    struct Temp { UINT CommandValue; TCmd Command; TEntry FirstEntry; };

    Temp const* pPtr = reinterpret_cast<Temp const*>(pPtrToCommandValue);
    entries = pPtr->Command.*NumEntries ? &pPtr->FirstEntry : nullptr;
    
    // Manually align the pointer to the next command.
    pPtrToCommandValue = BatchedContext::AlignPtr(&pPtr->FirstEntry + pPtr->Command.*NumEntries);
    return pPtr->Command;
}

template <UINT CommandValue> struct CommandDispatcher;
template <> struct CommandDispatcher<BatchedContext::CmdSetPipelineState::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetPipelineState>(pCommandData);
        ImmCtx.SetPipelineState(Data.pPSO);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDrawInstanced::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDrawInstanced>(pCommandData);
        ImmCtx.DrawInstanced(Data.countPerInstance, Data.instanceCount, Data.vertexStart, Data.instanceStart);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDrawIndexedInstanced::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDrawIndexedInstanced>(pCommandData);
        ImmCtx.DrawIndexedInstanced(Data.countPerInstance, Data.instanceCount, Data.indexStart, Data.vertexStart, Data.instanceStart);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDispatch::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDispatch>(pCommandData);
        ImmCtx.Dispatch(Data.x, Data.y, Data.z);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDrawAuto::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        (void)GetCommandData<BatchedContext::CmdDrawAuto>(pCommandData);
        ImmCtx.DrawAuto();
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDrawInstancedIndirect::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDrawInstancedIndirect>(pCommandData);
        ImmCtx.DrawInstancedIndirect(Data.pBuffer, Data.offset);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDrawIndexedInstancedIndirect::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDrawIndexedInstancedIndirect>(pCommandData);
        ImmCtx.DrawIndexedInstancedIndirect(Data.pBuffer, Data.offset);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDispatchIndirect::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdDispatchIndirect>(pCommandData);
        ImmCtx.DispatchIndirect(Data.pBuffer, Data.offset);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetTopology::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetTopology>(pCommandData);
        ImmCtx.IaSetTopology(Data.topology);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetVertexBuffers::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        struct Temp { BatchedContext::CmdSetVertexBuffers Cmd; Resource* pFirstVB; } const* pTemp = reinterpret_cast<Temp const*>(pCommandData);
        BatchedContext::CmdSetVertexBuffers const* pCmd = &pTemp->Cmd;
        auto ppVBs = &pTemp->pFirstVB;
        // Ptr guaranteed to be aligned because alignof(UINT) <= alignof(Resource*)
        auto pStrides = reinterpret_cast<UINT const*>(ppVBs + pCmd->numVBs);
        auto pOffsets = pStrides + pCmd->numVBs;
        // Align pointer to next command
        pCommandData = BatchedContext::AlignPtr(pOffsets + pCmd->numVBs);
        ImmCtx.IaSetVertexBuffers(pCmd->startSlot, pCmd->numVBs, ppVBs, pStrides, pOffsets);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetIndexBuffer::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetIndexBuffer>(pCommandData);
        ImmCtx.IaSetIndexBuffer(Data.pBuffer, Data.format, Data.offset);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetShaderResources::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        SRV* const* ppSRVs = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdSetShaderResources>(pCommandData, &BatchedContext::CmdSetShaderResources::numSRVs, ppSRVs);
        switch (Data.stage)
        {
        case e_VS: ImmCtx.SetShaderResources<e_VS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        case e_PS: ImmCtx.SetShaderResources<e_PS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        case e_GS: ImmCtx.SetShaderResources<e_GS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        case e_HS: ImmCtx.SetShaderResources<e_HS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        case e_DS: ImmCtx.SetShaderResources<e_DS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        case e_CS: ImmCtx.SetShaderResources<e_CS>(Data.startSlot, Data.numSRVs, ppSRVs); break;
        }
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetSamplers::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        Sampler* const* ppSamplers = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdSetSamplers>(pCommandData, &BatchedContext::CmdSetSamplers::numSamplers, ppSamplers);
        switch (Data.stage)
        {
        case e_VS: ImmCtx.SetSamplers<e_VS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        case e_PS: ImmCtx.SetSamplers<e_PS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        case e_GS: ImmCtx.SetSamplers<e_GS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        case e_HS: ImmCtx.SetSamplers<e_HS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        case e_DS: ImmCtx.SetSamplers<e_DS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        case e_CS: ImmCtx.SetSamplers<e_CS>(Data.startSlot, Data.numSamplers, ppSamplers); break;
        }
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetConstantBuffers::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        struct Temp { BatchedContext::CmdSetConstantBuffers Cmd; Resource* pFirstCB; } const* pTemp = reinterpret_cast<Temp const*>(pCommandData);
        BatchedContext::CmdSetConstantBuffers const* pCmd = &pTemp->Cmd;
        auto ppCBs = &pTemp->pFirstCB;
        // Ptr guaranteed to be aligned because alignof(UINT) <= alignof(Resource*)
        auto pFirstConstant = reinterpret_cast<UINT const*>(ppCBs + pCmd->numCBs);
        auto pNumConstants = pFirstConstant + pCmd->numCBs;
        // Align pointer to next command
        pCommandData = BatchedContext::AlignPtr(pNumConstants + pCmd->numCBs);
        switch (pCmd->stage)
        {
        case e_VS: ImmCtx.SetConstantBuffers<e_VS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        case e_PS: ImmCtx.SetConstantBuffers<e_PS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        case e_GS: ImmCtx.SetConstantBuffers<e_GS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        case e_HS: ImmCtx.SetConstantBuffers<e_HS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        case e_DS: ImmCtx.SetConstantBuffers<e_DS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        case e_CS: ImmCtx.SetConstantBuffers<e_CS>(pCmd->startSlot, pCmd->numCBs, ppCBs, pFirstConstant, pNumConstants); break;
        }
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetConstantBuffersNullOffsetSize::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        Resource* const* ppCBs = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdSetConstantBuffersNullOffsetSize>(pCommandData, &BatchedContext::CmdSetConstantBuffersNullOffsetSize::numCBs, ppCBs);
        switch (Data.stage)
        {
        case e_VS: ImmCtx.SetConstantBuffers<e_VS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        case e_PS: ImmCtx.SetConstantBuffers<e_PS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        case e_GS: ImmCtx.SetConstantBuffers<e_GS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        case e_HS: ImmCtx.SetConstantBuffers<e_HS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        case e_DS: ImmCtx.SetConstantBuffers<e_DS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        case e_CS: ImmCtx.SetConstantBuffers<e_CS>(Data.startSlot, Data.numCBs, ppCBs, nullptr, nullptr); break;
        }
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetSOBuffers::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetSOBuffers>(pCommandData);
        ImmCtx.SoSetTargets(4, 0, Data.pBuffers, Data.offsets);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetRenderTargets::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetRenderTargets>(pCommandData);
        ImmCtx.OMSetRenderTargets(Data.pRTVs, 8, Data.pDSV);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetUAV::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetUAV>(pCommandData);
        if (Data.graphics)
        {
            ImmCtx.OMSetUnorderedAccessViews(Data.slot, 1, &Data.pUAV, &Data.initialCount);
        }
        else
        {
            ImmCtx.CsSetUnorderedAccessViews(Data.slot, 1, &Data.pUAV, &Data.initialCount);
        }
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetStencilRef::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetStencilRef>(pCommandData);
        ImmCtx.OMSetStencilRef(Data.ref);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetBlendFactor::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetBlendFactor>(pCommandData);
        ImmCtx.OMSetBlendFactor(Data.factor);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetViewport::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetViewport>(pCommandData);
        ImmCtx.SetViewport(Data.slot, &Data.viewport);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetNumViewports::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetNumViewports>(pCommandData);
        ImmCtx.SetNumViewports(Data.num);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetScissorRect::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetScissorRect>(pCommandData);
        ImmCtx.SetScissorRect(Data.slot, &Data.rect);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetNumScissorRects::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetNumScissorRects>(pCommandData);
        ImmCtx.SetNumScissorRects(Data.num);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetScissorEnable::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetScissorEnable>(pCommandData);
        ImmCtx.SetScissorRectEnable(Data.enable);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearRenderTargetView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearRenderTargetView>(pCommandData, &BatchedContext::CmdClearRenderTargetView::numRects, pRects);
        ImmCtx.ClearRenderTargetView(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearDepthStencilView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearDepthStencilView>(pCommandData, &BatchedContext::CmdClearDepthStencilView::numRects, pRects);
        ImmCtx.ClearDepthStencilView(Data.pView, Data.flags, Data.depth, Data.stencil, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearUnorderedAccessViewUint::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearUnorderedAccessViewUint>(pCommandData, &BatchedContext::CmdClearUnorderedAccessViewUint::numRects, pRects);
        ImmCtx.ClearUnorderedAccessViewUint(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearUnorderedAccessViewFloat::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearUnorderedAccessViewFloat>(pCommandData, &BatchedContext::CmdClearUnorderedAccessViewFloat::numRects, pRects);
        ImmCtx.ClearUnorderedAccessViewFloat(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearVideoDecoderOutputView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearVideoDecoderOutputView>(pCommandData, &BatchedContext::CmdClearVideoDecoderOutputView::numRects, pRects);
        ImmCtx.ClearVideoDecoderOutputView(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearVideoProcessorInputView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearVideoProcessorInputView>(pCommandData, &BatchedContext::CmdClearVideoProcessorInputView::numRects, pRects);
        ImmCtx.ClearVideoProcessorInputView(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearVideoProcessorOutputView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdClearVideoProcessorOutputView>(pCommandData, &BatchedContext::CmdClearVideoProcessorOutputView::numRects, pRects);
        ImmCtx.ClearVideoProcessorOutputView(Data.pView, Data.color, Data.numRects, pRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDiscardView::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdDiscardView>(pCommandData, &BatchedContext::CmdDiscardView::numRects, pRects);
        ImmCtx.DiscardView(Data.pView, pRects, Data.numRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdDiscardResource::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        D3D12_RECT const* pRects = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdDiscardResource>(pCommandData, &BatchedContext::CmdDiscardResource::numRects, pRects);
        ImmCtx.DiscardResource(Data.pResource, pRects, Data.numRects);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdGenMips::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdGenMips>(pCommandData);
        ImmCtx.GenMips(Data.pSRV, Data.filterType);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdFinalizeUpdateSubresources::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdFinalizeUpdateSubresources>(pCommandData);
        ImmCtx.FinalizeUpdateSubresources(Data.pDst, Data.Op, nullptr);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdFinalizeUpdateSubresourcesWithLocalPlacement::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdFinalizeUpdateSubresourcesWithLocalPlacement>(pCommandData);
        ImmCtx.FinalizeUpdateSubresources(Data.pDst, Data.Op.Base, Data.Op.LocalPlacementDescs);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdRename::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdRename>(pCommandData);
        ImmCtx.Rename(Data.pResource, Data.pRenameResource);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdRenameViaCopy::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdRenameViaCopy>(pCommandData);
        ImmCtx.RenameViaCopy(Data.pResource, Data.pRenameResource, Data.dirtyPlaneMask);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdQueryBegin::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdQueryBegin>(pCommandData);
        ImmCtx.QueryBegin(Data.pQuery);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdQueryEnd::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdQueryEnd>(pCommandData);
        ImmCtx.QueryEnd(Data.pQuery);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetPredication::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetPredication>(pCommandData);
        ImmCtx.SetPredication(Data.pPredicate, Data.Value);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdResourceCopy::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdResourceCopy>(pCommandData);
        ImmCtx.ResourceCopy(Data.pDst, Data.pSrc);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdResolveSubresource::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdResolveSubresource>(pCommandData);
        ImmCtx.ResourceResolveSubresource(Data.pDst, Data.DstSubresource, Data.pSrc, Data.SrcSubresource, Data.Format);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdResourceCopyRegion::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdResourceCopyRegion>(pCommandData);
        ImmCtx.ResourceCopyRegion(Data.pDst, Data.DstSubresource, Data.DstX, Data.DstY, Data.DstZ, Data.pSrc, Data.SrcSubresource, &Data.SrcBox);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetResourceMinLOD::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetResourceMinLOD>(pCommandData);
        ImmCtx.SetResourceMinLOD(Data.pResource, Data.MinLOD);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdCopyStructureCount::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdCopyStructureCount>(pCommandData);
        ImmCtx.CopyStructureCount(Data.pDst, Data.DstOffset, Data.pSrc);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdRotateResourceIdentities::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        Resource* const* ppResources = nullptr;
        auto& Data = GetCommandDataVariableSize<BatchedContext::CmdRotateResourceIdentities>(pCommandData, &BatchedContext::CmdRotateResourceIdentities::NumResources, ppResources);
        ImmCtx.RotateResourceIdentities(ppResources, Data.NumResources);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdExtension::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto pData = reinterpret_cast<BatchedContext::CmdExtension const*>(pCommandData);
        const void* pExtensionData = BatchedContext::AlignPtr(pData + 1);
        pCommandData = reinterpret_cast<const BYTE*>(pExtensionData) + pData->DataSize;
        pData->pExt->Dispatch(ImmCtx, pExtensionData, pData->DataSize);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetHardwareProtection::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetHardwareProtection>(pCommandData);
        ImmCtx.SetHardwareProtection(Data.pResource, Data.Value);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetHardwareProtectionState::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdSetHardwareProtectionState>(pCommandData);
        ImmCtx.SetHardwareProtectionState(Data.State);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdClearState::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        (void)GetCommandData<BatchedContext::CmdClearState>(pCommandData);
        ImmCtx.ClearState();
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdUpdateTileMappings::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        struct Temp { BatchedContext::CmdUpdateTileMappings Cmd; D3D12_TILED_RESOURCE_COORDINATE Coords; } const* pTemp = reinterpret_cast<Temp const*>(pCommandData);
        static_assert(alignof(D3D12_TILED_RESOURCE_COORDINATE) == alignof(UINT));
        static_assert(alignof(D3D12_TILE_REGION_SIZE) == alignof(UINT));

        BatchedContext::CmdUpdateTileMappings const* pCmd = &pTemp->Cmd;
        // Note: Memory for all arrays is unconditionally allocated, even if null pointers are provided, for pointer math simplicity.
        D3D12_TILED_RESOURCE_COORDINATE const* pCoords = &pTemp->Coords;
        auto pRegions = reinterpret_cast<D3D12_TILE_REGION_SIZE const*>(pCoords + pCmd->NumTiledResourceRegions);
        auto pRangeFlags = reinterpret_cast<ImmediateContext::TILE_RANGE_FLAG const*>(pRegions + pCmd->NumTiledResourceRegions);
        auto pTilePoolStartOffsets = reinterpret_cast<const UINT*>(pRangeFlags + pCmd->NumRanges);
        auto pRangeTileCounts = pTilePoolStartOffsets + pCmd->NumRanges;
        pCommandData = BatchedContext::AlignPtr(pRangeTileCounts + pCmd->NumRanges);

        ImmCtx.UpdateTileMappings(pCmd->pTiledResource,
                                  pCmd->NumTiledResourceRegions,
                                  pCoords,
                                  pCmd->bTiledResourceRegionSizesPresent ? pRegions : nullptr,
                                  pCmd->pTilePool,
                                  pCmd->NumRanges,
                                  pCmd->bRangeFlagsPresent ? pRangeFlags : nullptr,
                                  pCmd->bTilePoolStartOffsetsPresent ? pTilePoolStartOffsets : nullptr,
                                  pCmd->bRangeTileCountsPresent ? pRangeTileCounts : nullptr,
                                  pCmd->Flags);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdCopyTileMappings::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdCopyTileMappings>(pCommandData);
        ImmCtx.CopyTileMappings(Data.pDstTiledResource, &Data.DstStartCoords, Data.pSrcTiledResource, &Data.SrcStartCoords, &Data.TileRegion, Data.Flags);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdCopyTiles::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdCopyTiles>(pCommandData);
        ImmCtx.CopyTiles(Data.pResource, &Data.StartCoords, &Data.TileRegion, Data.pBuffer, Data.BufferOffset, Data.Flags);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdTiledResourceBarrier::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdTiledResourceBarrier>(pCommandData);
        ImmCtx.TiledResourceBarrier(Data.pBefore, Data.pAfter);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdResizeTilePool::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdResizeTilePool>(pCommandData);
        ImmCtx.ResizeTilePool(Data.pTilePool, Data.NewSize);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdExecuteNestedBatch::CmdValue>
{
    static void Execute(ImmediateContext&, const void*& pCommandData)
    {
        auto& Data = GetCommandData<BatchedContext::CmdExecuteNestedBatch>(pCommandData);
        // Note: The D3D11 runtime also recursively executes nested command lists, so this is probably safe to do.
        Data.pThis->ProcessBatchImpl(Data.pBatch);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdSetMarker::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        const wchar_t* name = nullptr;
        (void)GetCommandDataVariableSize<BatchedContext::CmdSetMarker>(pCommandData, &BatchedContext::CmdSetMarker::NumChars, name);
        ImmCtx.SetMarker(name);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdBeginEvent::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        const wchar_t* name = nullptr;
        (void)GetCommandDataVariableSize<BatchedContext::CmdBeginEvent>(pCommandData, &BatchedContext::CmdBeginEvent::NumChars, name);
        ImmCtx.BeginEvent(name);
    }
};
template <> struct CommandDispatcher<BatchedContext::CmdEndEvent::CmdValue>
{
    static void Execute(ImmediateContext& ImmCtx, const void*& pCommandData)
    {
        (void)GetCommandData<BatchedContext::CmdEndEvent>(pCommandData);
        ImmCtx.EndEvent();
    }
};

//----------------------------------------------------------------------------------------------------------------------------------
// These structs generate an array consisting of the functions defined above,
// in order of their command value, so it can be indexed by value as a dispatch table.
template <int N, int... Rest>
struct DispatchArrayImpl
{
    // Generic case, just grab value from below.
    // This is recursive, so Rest starts from empty, to N, to (N-1, N), etc, until 0 is the first value.
    // Then it triggers the base.
    static constexpr auto& value = DispatchArrayImpl<N - 1, N, Rest...>::value;
};

template <int... Rest>
struct DispatchArrayImpl<0, Rest...>
{
    // Base case, define array.
    static constexpr BatchedContext::DispatcherFunction value[] = { &CommandDispatcher<0>::Execute, &CommandDispatcher<Rest>::Execute... };
};

// Statics need to be explicitly instantiated.
template <int... Rest>
constexpr BatchedContext::DispatcherFunction DispatchArrayImpl<0, Rest...>::value[];

// Instantiate with the correct number of commands. Note that the array generated is inclusive, not exclusive.
constexpr auto& DispatchArray = DispatchArrayImpl<BatchedContext::c_LastCommand>::value;

//----------------------------------------------------------------------------------------------------------------------------------
BatchedContext::BatchedContext(ImmediateContext& ImmCtx, CreationArgs args, Callbacks const& callbacks)
    : m_ImmCtx(ImmCtx) // WARNING: ImmCtx might not be initialized yet, avoid any access to it during this constructor.
    , m_CreationArgs(args)
    , m_DispatchTable(DispatchArray)
    , m_Callbacks(callbacks)
{
    if (args.SubmitBatchesToWorkerThread)
    {
        m_BatchSubmittedSemaphore.m_h = CreateSemaphore(nullptr, 0, c_MaxOutstandingBatches, nullptr);
        ThrowIfHandleNull(m_BatchSubmittedSemaphore);

        m_BatchConsumedSemaphore.m_h = CreateSemaphore(nullptr, 0, c_MaxOutstandingBatches, nullptr);
        ThrowIfHandleNull(m_BatchConsumedSemaphore);

        m_BatchThread.m_h = CreateThread(
            nullptr, 0,
            [](void* pContext) -> DWORD
        {
            reinterpret_cast<BatchedContext*>(pContext)->BatchThread();
            return 0;
        }, this, CREATE_SUSPENDED, nullptr);
        ThrowIfHandleNull(m_BatchThread);
        ResumeThread(m_BatchThread.m_h);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
BatchedContext::~BatchedContext()
{
    assert(!IsBatchThread());
    // Batch producing contexts shouldn't flush on their own.
    if (!m_CreationArgs.pParentContext)
    {
        ProcessBatch();
    }
    if (m_CreationArgs.SubmitBatchesToWorkerThread)
    {
        assert(m_NumOutstandingBatches == 0 && m_QueuedBatches.empty());

        // When the batch thread wakes up after consuming a semaphore value, and
        // sees that the queue is empty, it will exit.
        BOOL value = ReleaseSemaphore(m_BatchSubmittedSemaphore, 1, nullptr);
        assert(value == TRUE);
        UNREFERENCED_PARAMETER(value);

        // Wait for it to exit.
        WaitForSingleObject(m_BatchThread, INFINITE);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TCmd>
void BatchedContext::AddToBatch(BatchStorage& CurrentBatch, TCmd const& command)
{
    assert(!IsBatchThread());
    static_assert(std::is_trivially_destructible<TCmd>::value, "Destructors don't get called on batched commands.");
    struct alignas(BatchPrimitive)Temp { UINT CommandValue; TCmd Command; };

    if (!CurrentBatch.reserve_contiguous(sizeof(Temp) / sizeof(BatchPrimitive)))
    {
        throw std::bad_alloc();
    }

    Temp* pPtr = reinterpret_cast<Temp*>(CurrentBatch.append_contiguous_manually(sizeof(Temp) / sizeof(BatchPrimitive)));
    pPtr->CommandValue = TCmd::CmdValue;
    pPtr->Command = command;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TCmd, typename TEntry>
void BatchedContext::AddToBatchVariableSize(TCmd const& command, UINT NumEntries, TEntry const* entries)
{
    assert(!IsBatchThread());
    auto Lock = m_RecordingLock.TakeLock();
    static_assert(std::is_trivially_destructible<TCmd>::value, "Destructors don't get called on batched commands.");
    struct Temp { UINT CommandValue; TCmd Command; TEntry FirstEntry; };

    const size_t TotalSizeInBytes = Align(offsetof(Temp, FirstEntry) + sizeof(TEntry) * (NumEntries), sizeof(BatchPrimitive));
    const size_t TotalSizeInElements = TotalSizeInBytes / sizeof(BatchPrimitive);

    if (!m_CurrentBatch.reserve_contiguous(TotalSizeInElements))
    {
        throw std::bad_alloc();
    }

    Temp* pPtr = reinterpret_cast<Temp*>(m_CurrentBatch.append_contiguous_manually(TotalSizeInElements));
    pPtr->CommandValue = TCmd::CmdValue;
    pPtr->Command = command;
    std::copy(entries, entries + NumEntries, &pPtr->FirstEntry);

    ++m_CurrentCommandCount;
    SubmitBatchIfIdle();
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TCmd, typename... Args>
void BatchedContext::EmplaceInBatch(Args&&... args)
{
    assert(!IsBatchThread());
    auto Lock = m_RecordingLock.TakeLock();
    static_assert(std::is_trivially_destructible<TCmd>::value, "Destructors don't get called on batched commands.");
    const size_t CommandSize = TCmd::GetCommandSize(std::forward<Args>(args)...);
    // GetCommandSize must ensure size is aligned correctly.
    assert(CommandSize % sizeof(BatchPrimitive) == 0);

    if (!m_CurrentBatch.reserve_contiguous(CommandSize / sizeof(BatchPrimitive)))
    {
        throw std::bad_alloc();
    }

    void* pPtr = m_CurrentBatch.append_contiguous_manually(CommandSize / sizeof(BatchPrimitive));
    new (pPtr) TCmd(std::forward<Args>(args)...);

    ++m_CurrentCommandCount;
    SubmitBatchIfIdle();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetPipelineState(PipelineState* pPSO)
{
    AddToBatch(CmdSetPipelineState{ pPSO });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DrawInstanced(UINT countPerInstance, UINT instanceCount, UINT vertexStart, UINT instanceStart)
{
    AddToBatch(CmdDrawInstanced{ countPerInstance, instanceCount, vertexStart, instanceStart });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DrawIndexedInstanced(UINT countPerInstance, UINT instanceCount, UINT indexStart, INT vertexStart, UINT instanceStart)
{
    AddToBatch(CmdDrawIndexedInstanced{ countPerInstance, instanceCount, indexStart, vertexStart, instanceStart });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DrawAuto()
{
    AddToBatch(CmdDrawAuto{});
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DrawIndexedInstancedIndirect(Resource* pBuffer, UINT offset)
{
    AddToBatch(CmdDrawIndexedInstancedIndirect{ pBuffer, offset });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DrawInstancedIndirect(Resource* pBuffer, UINT offset)
{
    AddToBatch(CmdDrawInstancedIndirect{ pBuffer, offset });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::Dispatch(UINT x, UINT y, UINT z)
{
    AddToBatch(CmdDispatch{ x, y, z });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DispatchIndirect(Resource* pBuffer, UINT offset)
{
    AddToBatch(CmdDispatchIndirect{ pBuffer, offset });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::IaSetTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
    AddToBatch(CmdSetTopology{ topology });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::IaSetVertexBuffers(UINT StartSlot, __in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) UINT NumBuffers, Resource** pVBs, const UINT*pStrides, const UINT* pOffsets)
{
    EmplaceInBatch<CmdSetVertexBuffers>(StartSlot, NumBuffers, pVBs, pStrides, pOffsets);
}

BatchedContext::CmdSetVertexBuffers::CmdSetVertexBuffers(UINT _startSlot, UINT _numVBs, Resource* const* _ppVBs, UINT const* _pStrides, UINT const* _pOffsets)
    : startSlot(_startSlot)
    , numVBs(_numVBs)
{
    struct Temp { BatchedContext::CmdSetVertexBuffers Cmd; Resource* pFirstVB; } *pTemp = reinterpret_cast<Temp*>(this);
    auto ppVBs = &pTemp->pFirstVB;
    // Ptr guaranteed to be aligned because alignof(UINT) <= alignof(Resource*)
    auto pStrides = reinterpret_cast<UINT*>(ppVBs + numVBs);
    auto pOffsets = pStrides + numVBs;
    std::copy(_ppVBs, _ppVBs + numVBs, ppVBs);
    std::copy(_pStrides, _pStrides + numVBs, pStrides);
    std::copy(_pOffsets, _pOffsets + numVBs, pOffsets);
}

size_t BatchedContext::CmdSetVertexBuffers::GetCommandSize(UINT, UINT _numVBs, Resource* const*, UINT const*, UINT const*)
{
    struct Temp { BatchedContext::CmdSetVertexBuffers Cmd; Resource* pFirstVB; };
    return Align(offsetof(Temp, pFirstVB) + (sizeof(Resource*) + sizeof(UINT) * 2) * _numVBs, sizeof(BatchPrimitive));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::IaSetIndexBuffer(Resource* pBuffer, DXGI_FORMAT format, UINT offset)
{
    AddToBatch(CmdSetIndexBuffer{ pBuffer, format, offset });
}

//----------------------------------------------------------------------------------------------------------------------------------
template <EShaderStage ShaderStage>
void TRANSLATION_API BatchedContext::SetShaderResources(UINT StartSlot, __in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) UINT NumSRVs, SRV* const* ppSRVs)
{
    AddToBatchVariableSize(CmdSetShaderResources{ ShaderStage, StartSlot, NumSRVs }, NumSRVs, ppSRVs);
}
template void TRANSLATION_API BatchedContext::SetShaderResources<e_VS>(UINT, UINT, SRV* const*);
template void TRANSLATION_API BatchedContext::SetShaderResources<e_PS>(UINT, UINT, SRV* const*);
template void TRANSLATION_API BatchedContext::SetShaderResources<e_GS>(UINT, UINT, SRV* const*);
template void TRANSLATION_API BatchedContext::SetShaderResources<e_HS>(UINT, UINT, SRV* const*);
template void TRANSLATION_API BatchedContext::SetShaderResources<e_DS>(UINT, UINT, SRV* const*);
template void TRANSLATION_API BatchedContext::SetShaderResources<e_CS>(UINT, UINT, SRV* const*);

//----------------------------------------------------------------------------------------------------------------------------------
template <EShaderStage ShaderStage>
void TRANSLATION_API BatchedContext::SetSamplers(UINT StartSlot, __in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) UINT NumSamplers, Sampler** ppSamplers)
{
    AddToBatchVariableSize(CmdSetSamplers{ ShaderStage, StartSlot, NumSamplers }, NumSamplers, ppSamplers );
}
template void TRANSLATION_API BatchedContext::SetSamplers<e_VS>(UINT, UINT, Sampler**);
template void TRANSLATION_API BatchedContext::SetSamplers<e_PS>(UINT, UINT, Sampler**);
template void TRANSLATION_API BatchedContext::SetSamplers<e_GS>(UINT, UINT, Sampler**);
template void TRANSLATION_API BatchedContext::SetSamplers<e_HS>(UINT, UINT, Sampler**);
template void TRANSLATION_API BatchedContext::SetSamplers<e_DS>(UINT, UINT, Sampler**);
template void TRANSLATION_API BatchedContext::SetSamplers<e_CS>(UINT, UINT, Sampler**);

//----------------------------------------------------------------------------------------------------------------------------------
template <EShaderStage ShaderStage>
void TRANSLATION_API BatchedContext::SetConstantBuffers(UINT StartSlot, __in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT) UINT NumBuffers, Resource** ppCBs, __in_ecount_opt(NumBuffers) CONST UINT* pFirstConstant, __in_ecount_opt(NumBuffers) CONST UINT* pNumConstants)
{
    if (pFirstConstant)
    {
        EmplaceInBatch<CmdSetConstantBuffers>(ShaderStage, StartSlot, NumBuffers, ppCBs, pFirstConstant, pNumConstants);
    }
    else
    {
        AddToBatchVariableSize(CmdSetConstantBuffersNullOffsetSize{ ShaderStage, StartSlot, NumBuffers }, NumBuffers, ppCBs);
    }
}
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_VS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_PS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_GS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_HS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_DS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);
template void TRANSLATION_API BatchedContext::SetConstantBuffers<e_CS>(UINT, UINT, Resource** ppCBs, CONST UINT* pFirstConstant, CONST UINT* pNumConstants);

BatchedContext::CmdSetConstantBuffers::CmdSetConstantBuffers(EShaderStage _stage, UINT _startSlot, UINT _numCBs, Resource* const* _ppCBs, UINT const* _pFirstConstant, UINT const* _pNumConstants)
    : stage(_stage)
    , startSlot(_startSlot)
    , numCBs(_numCBs)
{
    struct Temp { BatchedContext::CmdSetConstantBuffers Cmd; Resource* pFirstCB; } *pTemp = reinterpret_cast<Temp*>(this);
    auto ppCBs = &pTemp->pFirstCB;
    // Ptr guaranteed to be aligned because alignof(UINT) <= alignof(Resource*)
    auto pFirstConstant = reinterpret_cast<UINT*>(ppCBs + numCBs);
    auto pNumConstants = pFirstConstant + numCBs;
    std::copy(_ppCBs, _ppCBs + numCBs, ppCBs);
    std::copy(_pFirstConstant, _pFirstConstant + numCBs, pFirstConstant);
    std::copy(_pNumConstants, _pNumConstants + numCBs, pNumConstants);
}

size_t BatchedContext::CmdSetConstantBuffers::GetCommandSize(EShaderStage, UINT, UINT numCBs, Resource* const*, UINT const*, UINT const*)
{
    struct Temp { BatchedContext::CmdSetConstantBuffers Cmd; Resource* pFirstCB; };
    return Align(offsetof(Temp, pFirstCB) + (sizeof(Resource*) + sizeof(UINT) * 2) * numCBs, sizeof(BatchPrimitive));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SoSetTargets(_In_range_(0, 4) UINT NumTargets, _In_range_(0, 4) UINT, _In_reads_(NumTargets) Resource** pBuffers, _In_reads_(NumTargets) const UINT* offsets)
{
    CmdSetSOBuffers command = {};
    std::copy(pBuffers, pBuffers + NumTargets, command.pBuffers);
    std::copy(offsets, offsets + NumTargets, command.offsets);
    AddToBatch(command);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::OMSetRenderTargets(__in_ecount(NumRTVs) RTV** ppRTVs, __in_range(0, 8) UINT NumRTVs, __in_opt DSV *pDSV, __in_ecount(NumUavs) UAV** ppUavs, CONST UINT* pInitialCounts, UINT UAVStartSlot, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumUavs)
{
    CmdSetRenderTargets command = {};
    std::copy(ppRTVs, ppRTVs + NumRTVs, command.pRTVs);
    command.pDSV = pDSV;
    AddToBatch(command);

    for (UINT i = 0; i < D3D11_1_UAV_SLOT_COUNT; ++i)
    {
        UINT slot = i;
        UINT inputIndex = slot - UAVStartSlot;
        bool bValidUAVSlot = slot >= NumRTVs && inputIndex < NumUavs;
        UAV* pUAV = bValidUAVSlot ? ppUavs[inputIndex] : nullptr;

        if (m_UAVs.UpdateBinding(slot, pUAV) || (pUAV && pInitialCounts[inputIndex] != UINT_MAX))
        {
            AddToBatch(CmdSetUAV{ true, slot, pUAV, pUAV ? pInitialCounts[inputIndex] : UINT_MAX });
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::CsSetUnorderedAccessViews(UINT Start, __in_range(0, D3D11_1_UAV_SLOT_COUNT) UINT NumViews, __in_ecount(NumViews) UAV** ppUAVs, __in_ecount(NumViews) CONST UINT* pInitialCounts)
{
    for (UINT i = 0; i < NumViews; ++i)
    {
        AddToBatch(CmdSetUAV{ false, i + Start, ppUAVs[i], pInitialCounts ? pInitialCounts[i] : UINT_MAX });
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::OMSetStencilRef(UINT StencilRef)
{
    AddToBatch(CmdSetStencilRef{ StencilRef });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
    AddToBatch(CmdSetBlendFactor{ {BlendFactor[0], BlendFactor[1], BlendFactor[2], BlendFactor[3]} });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
    for (UINT i = 0; i < NumViewports; ++i)
    {
        if (memcmp(&pViewports[i], &m_Viewports[i], sizeof(D3D12_VIEWPORT)) != 0)
        {
            m_Viewports[i] = pViewports[i];
            AddToBatch(CmdSetViewport{ i, pViewports[i] });
        }
    }
    if (m_NumViewports != NumViewports)
    {
        AddToBatch(CmdSetNumViewports{ NumViewports });
        m_NumViewports = NumViewports;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
    for (UINT i = 0; i < NumRects; ++i)
    {
        if (memcmp(&pRects[i], &m_Scissors[i], sizeof(D3D12_RECT)) != 0)
        {
            m_Scissors[i] = pRects[i];
            AddToBatch(CmdSetScissorRect{ i, pRects[i] });
        }
    }
    if (m_NumScissors != NumRects)
    {
        AddToBatch(CmdSetNumScissorRects{ NumRects });
        m_NumScissors = NumRects;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetScissorRectEnable(BOOL enable)
{
    AddToBatch(CmdSetScissorEnable{ enable != 0 });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearRenderTargetView(RTV* pRTV, CONST FLOAT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearRenderTargetView{ pRTV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearDepthStencilView(DSV* pDSV, UINT Flags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearDepthStencilView{ pDSV, Flags, Depth, Stencil, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearUnorderedAccessViewUint(UAV* pUAV, CONST UINT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearUnorderedAccessViewUint{ pUAV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearUnorderedAccessViewFloat(UAV* pUAV, CONST FLOAT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearUnorderedAccessViewFloat{ pUAV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearVideoDecoderOutputView(VDOV* pVDOV, CONST FLOAT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearVideoDecoderOutputView{ pVDOV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearVideoProcessorInputView(VPIV* pVPIV, CONST FLOAT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearVideoProcessorInputView{ pVPIV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearVideoProcessorOutputView(VPOV* vPOV, CONST FLOAT Color[4], UINT NumRects, const D3D12_RECT *pRects)
{
    AddToBatchVariableSize(CmdClearVideoProcessorOutputView{ vPOV, Color, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DiscardView(ViewBase* pView, const D3D12_RECT* pRects, UINT NumRects)
{
    AddToBatchVariableSize(CmdDiscardView{ pView, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::DiscardResource(Resource* pResource, const D3D12_RECT* pRects, UINT NumRects)
{
    AddToBatchVariableSize(CmdDiscardResource{ pResource, NumRects }, NumRects, pRects);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::GenMips(SRV* pSRV, D3D12_FILTER_TYPE FilterType)
{
    AddToBatch(CmdGenMips{ pSRV, FilterType });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ResourceUpdateSubresourceUP(Resource* pResource, UINT DstSubresource, _In_opt_ const D3D12_BOX* pDstBox, _In_ const VOID* pMem, UINT SrcPitch, UINT SrcDepth)
{
    D3D11_SUBRESOURCE_DATA SubresourceDesc = { pMem, SrcPitch, SrcDepth };
    UINT8 MipLevel, PlaneSlice;
    UINT16 ArraySlice;
    DecomposeSubresourceIdxExtended(DstSubresource, pResource->AppDesc()->MipLevels(), pResource->AppDesc()->ArraySize(), MipLevel, ArraySlice, PlaneSlice);

    const ImmediateContext::CPrepareUpdateSubresourcesHelper PrepareHelper(
        *pResource,
        CSubresourceSubset(1, 1, pResource->SubresourceMultiplier(), MipLevel, ArraySlice, PlaneSlice),
        &SubresourceDesc,
        pDstBox,
        ImmediateContext::UpdateSubresourcesFlags::ScenarioBatchedContext,
        nullptr, 0, m_ImmCtx);

    if (PrepareHelper.FinalizeNeeded) // Might be a no-op due to box.
    {
        if (PrepareHelper.bUseLocalPlacement)
        {
            AddToBatch(CmdFinalizeUpdateSubresourcesWithLocalPlacement{ pResource, PrepareHelper.PreparedStorage });
        }
        else
        {
            AddToBatch(CmdFinalizeUpdateSubresources{ pResource, PrepareHelper.PreparedStorage.Base });
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
_Use_decl_annotations_
void TRANSLATION_API BatchedContext::UploadInitialData(Resource* pDst, D3D12TranslationLayer::CSubresourceSubset const& Subresources, const D3D11_SUBRESOURCE_DATA* pSrcData, const D3D12_BOX* pDstBox)
{
    const ImmediateContext::CPrepareUpdateSubresourcesHelper PrepareHelper(
        *pDst,
        Subresources,
        pSrcData,
        pDstBox,
        ImmediateContext::UpdateSubresourcesFlags::ScenarioInitialData,
        nullptr,
        0,
        m_ImmCtx);

    if (PrepareHelper.FinalizeNeeded) // Might have been written directly to the destination.
    {
        auto AddToBatchImpl = [&]()
        {
            if (PrepareHelper.bUseLocalPlacement)
            {
                AddToBatch(CmdFinalizeUpdateSubresourcesWithLocalPlacement{ pDst, PrepareHelper.PreparedStorage });
            }
            else
            {
                AddToBatch(CmdFinalizeUpdateSubresources{ pDst, PrepareHelper.PreparedStorage.Base });
            }
        };
        AddToBatchImpl();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Make sure the batch is completed, then call into the immediate context to wait for the GPU.
bool TRANSLATION_API BatchedContext::MapUnderlyingSynchronize(BatchedResource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMappedSubresource)
{
    bool result = FlushBatchAndGetImmediateContext().MapUnderlyingSynchronize(pResource->m_pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
    return result;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Make sure the batch is completed, then call into the immediate context to figure out how to do the map.
bool TRANSLATION_API BatchedContext::MapDefault(BatchedResource* pResource, UINT Subresource, MAP_TYPE MapType, bool DoNotWait, _In_opt_ const D3D12_BOX *pReadWriteRange, MappedSubresource* pMappedSubresource)
{
    bool result = FlushBatchAndGetImmediateContext().MapDefault(pResource->m_pResource, Subresource, MapType, DoNotWait, pReadWriteRange, pMappedSubresource);
    return result;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Call thread-safe immediate context methods to acquire a mappable buffer, and queue a rename operation.
bool TRANSLATION_API BatchedContext::RenameAndMapBuffer(BatchedResource* pResource, MappedSubresource* pMappedSubresource)
{
    try
    {
        auto& ImmCtx = GetImmediateContextNoFlush();
        SafeRenameResourceCookie cookie(ImmCtx.CreateRenameCookie(pResource->m_pResource, ResourceAllocationContext::FreeThread));
    
        Resource* pRenameResource = cookie.Get();
        pResource->m_LastRenamedResource = pRenameResource->GetIdentity()->m_suballocation;
        
        D3D12_RANGE ReadRange = CD3DX12_RANGE(0, 0);
        void* pData = nullptr;
    
        ThrowFailure(pResource->m_LastRenamedResource.Map(0, &ReadRange, &pData));
        AddToBatch(CmdRename{ pResource->m_pResource, cookie.Get() });
        AddPostBatchFunction([cleanup = cookie.Detach(), &immCtx = GetImmediateContextNoFlush()](){ immCtx.DeleteRenameCookie(cleanup); });
    
        pMappedSubresource->pData = pData;
        pMappedSubresource->RowPitch = pResource->m_pResource->GetSubresourcePlacement(0).Footprint.RowPitch;
        pMappedSubresource->DepthPitch = pResource->m_pResource->DepthPitch(0);
    
        return true;
    }
    catch (_com_error& hrEx)
    {
        if (hrEx.Error() == E_OUTOFMEMORY)
        {
            (void)FlushBatchAndGetImmediateContext().MapDiscardBuffer(pResource->m_pResource, 0, MAP_TYPE_WRITE_DISCARD, false, nullptr, pMappedSubresource);
            pResource->m_LastRenamedResource = pResource->m_pResource->GetIdentity()->m_suballocation;
            return true;
        }
        throw;
    }

}

//----------------------------------------------------------------------------------------------------------------------------------
// Call thread-safe immediate context methods to acquire a mappable buffer - don't queue anything yet.
bool TRANSLATION_API BatchedContext::MapForRenameViaCopy(BatchedResource* pResource, UINT Subresource, MappedSubresource* pMappedSubresource)
{
    if (pResource->m_pResource->GetFormatEmulation() == FormatEmulation::YV12)
    {
        return FlushBatchAndGetImmediateContext().MapDynamicTexture(pResource->m_pResource, Subresource, MAP_TYPE_READWRITE, true, nullptr, pMappedSubresource);
    }
    else
    {
        assert (pResource->m_pResource->GetFormatEmulation() == FormatEmulation::None);
        
        UINT MipIndex, PlaneIndex, ArrayIndex;
        pResource->m_pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);
        auto& ImmCtx = GetImmediateContextNoFlush();

        if (!pResource->m_DynamicTexturePlaneData.AnyPlaneMapped())
        {
            SafeRenameResourceCookie cookie(ImmCtx.CreateRenameCookie(pResource->m_pResource, ResourceAllocationContext::FreeThread));

            Resource* pRenameResource = cookie.Get();
            pResource->m_LastRenamedResource = pRenameResource->GetIdentity()->m_suballocation;

            pResource->m_PendingRenameViaCopyCookie.Reset(cookie.Detach());
        }

        pResource->m_DynamicTexturePlaneData.m_MappedPlaneRefCount[PlaneIndex]++;
        pResource->m_DynamicTexturePlaneData.m_DirtyPlaneMask |= (1 << PlaneIndex);

        D3D12_RANGE ReadRange = CD3DX12_RANGE(0, 0);
        void* pData = nullptr;

        ThrowFailure(pResource->m_LastRenamedResource.Map(0, &ReadRange, &pData));

        pMappedSubresource->pData = 
            (BYTE*)pData +
            (pResource->m_pResource->GetSubresourcePlacement(Subresource).Offset -
             pResource->m_pResource->GetSubresourcePlacement(0).Offset);
        pMappedSubresource->RowPitch = pResource->m_pResource->GetSubresourcePlacement(Subresource).Footprint.RowPitch;
        pMappedSubresource->DepthPitch = pResource->m_pResource->DepthPitch(Subresource);

        return true;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
// Re-map the last-acquired buffer associated with a resource.
bool TRANSLATION_API BatchedContext::MapRenamedBuffer(BatchedResource* pResource, MappedSubresource* pMappedSubresource)
{
    D3D12_RANGE ReadRange = CD3DX12_RANGE(0, 0);
    void* pData = nullptr;

    ThrowFailure(pResource->m_LastRenamedResource.Map(0, &ReadRange, &pData));

    pMappedSubresource->pData = pData;
    pMappedSubresource->RowPitch = pResource->m_pResource->GetSubresourcePlacement(0).Footprint.RowPitch;
    pMappedSubresource->DepthPitch = pResource->m_pResource->DepthPitch(0);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
// Just unmap the renamed buffer.
void TRANSLATION_API BatchedContext::UnmapRenamedBuffer(BatchedResource* pResource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    D3D12_RANGE WriteRange =
        pReadWriteRange
        ? CD3DX12_RANGE(pReadWriteRange->left, pReadWriteRange->right)
        : CD3DX12_RANGE(0, static_cast<SIZE_T>(pResource->m_LastRenamedResource.GetBufferSuballocation().GetSize()));

    pResource->m_LastRenamedResource.Unmap(0, &WriteRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Map enforced synchronization, just forward to immediate context.
void TRANSLATION_API BatchedContext::UnmapDefault(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    GetImmediateContextNoFlush().UnmapDefault(pResource->m_pResource, Subresource, pReadWriteRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Map enforced synchronization, just forward to immediate context.
void TRANSLATION_API BatchedContext::UnmapStaging(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    GetImmediateContextNoFlush().UnmapUnderlyingStaging(pResource->m_pResource, Subresource, pReadWriteRange);
}

//----------------------------------------------------------------------------------------------------------------------------------
// Unmap the buffer and queue a rename-via-copy operation.
void TRANSLATION_API BatchedContext::UnmapAndRenameViaCopy(BatchedResource* pResource, UINT Subresource, _In_opt_ const D3D12_BOX *pReadWriteRange)
{
    UINT MipIndex, PlaneIndex, ArrayIndex;
    pResource->m_pResource->DecomposeSubresource(Subresource, MipIndex, ArrayIndex, PlaneIndex);

    assert(pResource->m_DynamicTexturePlaneData.m_MappedPlaneRefCount[PlaneIndex] == 1);
    pResource->m_DynamicTexturePlaneData.m_MappedPlaneRefCount[PlaneIndex]--;
    if (!pResource->m_DynamicTexturePlaneData.AnyPlaneMapped())
    {
        D3D12_RANGE WriteRange = pReadWriteRange
            ? CD3DX12_RANGE(pReadWriteRange->left, pReadWriteRange->right)
            : CD3DX12_RANGE(0, static_cast<SIZE_T>(pResource->m_LastRenamedResource.GetBufferSuballocation().GetSize()));

        pResource->m_LastRenamedResource.Unmap(0, &WriteRange);

        AddToBatch(CmdRenameViaCopy{ pResource->m_pResource, pResource->m_PendingRenameViaCopyCookie.Get(), pResource->m_DynamicTexturePlaneData.m_DirtyPlaneMask });
        AddPostBatchFunction([cleanup = pResource->m_PendingRenameViaCopyCookie.Detach(), &immCtx = GetImmediateContextNoFlush()](){ immCtx.DeleteRenameCookie(cleanup); });

        pResource->m_DynamicTexturePlaneData = {};
        pResource->m_LastRenamedResource.Reset();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::QueryBegin(BatchedQuery* pAsync)
{
    if (pAsync->m_CurrentState == Async::AsyncState::Begun)
    {
        QueryEnd(pAsync);
    }

    auto Lock = m_RecordingLock.TakeLock();
    pAsync->m_BatchReferenceID = m_RecordingBatchID;

    AddToBatch(CmdQueryBegin{ pAsync->GetImmediateNoFlush() });
    pAsync->m_CurrentState = Async::AsyncState::Begun;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::QueryEnd(BatchedQuery* pAsync)
{
    if (pAsync->m_CurrentState == Async::AsyncState::Ended &&
        pAsync->GetImmediateNoFlush()->RequiresBegin())
    {
        QueryBegin(pAsync);
    }

    auto Lock = m_RecordingLock.TakeLock();
    pAsync->m_BatchReferenceID = m_RecordingBatchID;

    AddToBatch(CmdQueryEnd{ pAsync->GetImmediateNoFlush() });
    pAsync->m_CurrentState = Async::AsyncState::Ended;
}

//----------------------------------------------------------------------------------------------------------------------------------
template <typename TFunc>
bool BatchedContext::SyncWithBatch(uint64_t& BatchID, bool DoNotFlush, TFunc&& GetImmObjectFenceValues)
{
    {
        auto RecordingLock = m_RecordingLock.TakeLock();
        assert(BatchID <= m_RecordingBatchID);

        constexpr uint64_t GenerationIDMask = 0xffffffff00000000ull;
        if ((BatchID & GenerationIDMask) < (m_RecordingBatchID & GenerationIDMask))
        {
            // The batch ID comes from a different "generation" of batches.
            // Essentially, an object which wants to wait for a specific batch to be done
            // likely needs to monitor all functions which modify its state.
            //
            // If a command list batch is executed, it could modify the object's state
            // in a way that doesn't update the batch ID to be monitored.
            // In that case, assume that it could've been modified in the batch currently
            // being recorded (or the last one if this one's empty)
            //
            // Note that each generation starts with ID 1, so this subtraction will never
            // roll backwards into a previous generation.
            BatchID = m_CurrentBatch.empty() ? m_RecordingBatchID - 1 : m_RecordingBatchID;
        }

        // Not even submitted yet.
        if (BatchID >= m_RecordingBatchID)
        {
            if (!DoNotFlush)
            {
                // Submit and request flush to GPU as soon as it's done.
                SubmitBatch(true);
            }
            // Theoretically we could avoid this, as the query might actually finish in the time
            // between here and the checks below, but this fails the conformance tests, so we'll
            // play it safe and just assume that can't happen.
            return false;
        }
    }

    auto SubmissionLock = m_SubmissionLock.TakeLock();

    if (m_CompletedBatchID < BatchID)
    {
        if (!DoNotFlush)
        {
            assert(!m_QueuedBatches.empty());

            // Make sure it's marked to flush when it's done.
            auto iter = std::find_if(m_QueuedBatches.begin(), m_QueuedBatches.end(),
                                     [&BatchID](std::unique_ptr<Batch> const& p)
            {
                return p->m_BatchID > BatchID;
            });
            assert(iter != m_QueuedBatches.begin());
            --iter;

            // We don't know what command list types to use on this timeline, so just request all.
            (*iter)->m_FlushRequestedMask |= COMMAND_LIST_TYPE_ALL_MASK;
        }
        return false;
    }

    UINT64 FenceValues[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
    GetImmObjectFenceValues(FenceValues);
    if (!DoNotFlush)
    {
        // Check for flush.
        UINT FlushMask = 0;
        for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
        {
            if (FenceValues[i] != 0 &&
                // Note: Last seen command list IDs might not be completely up-to-date
                // if immediate context was accessed directly, so we need a conservative >= here,
                // but after at least attempting to flush once, they will become up-to-date.
                FenceValues[i] >= m_ImmCtx.GetCommandListIDInterlockedRead((COMMAND_LIST_TYPE)i))
            {
                FlushMask |= (1 << i);
            }
        }

        if (FlushMask != 0)
        {
            // Not checking thread idle bit as we're already under the lock.
            if (m_QueuedBatches.empty())
            {
                m_ImmCtx.PrepForCommandQueueSync(FlushMask);
            }
            else
            {
                m_QueuedBatches.front()->m_FlushRequestedMask |= FlushMask;
            }
            return false;
        }
    }

    for (UINT i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; ++i)
    {
        if (m_ImmCtx.GetCompletedFenceValue((COMMAND_LIST_TYPE)i) < FenceValues[i])
        {
            // Note: Work may not have been flushed to GPU if we were called with DoNotFlush.
            // Either way, work isn't done.
            return false;
        }
    }

    // CPU and GPU are done with it.
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API BatchedContext::QueryGetData(BatchedQuery* pAsync, void* pData, UINT DataSize, bool DoNotFlush)
{
    // Make sure it's in the right state (might update batch reference)
    if (pAsync->m_CurrentState == Async::AsyncState::Begun)
    {
        QueryEnd(pAsync);
    }

    // Check if it's done yet.
    if (!SyncWithBatch(pAsync->m_BatchReferenceID, DoNotFlush,
                       [pAsync](UINT64* pFenceValues)
    {
        auto& EndedCommandListIDs = pAsync->GetImmediateNoFlush()->m_EndedCommandListID;
        std::copy(EndedCommandListIDs, std::end(EndedCommandListIDs), pFenceValues);
    }))
    {
        return false;
    }

    ImmediateContext& ImmCtx = GetImmediateContextNoFlush();
    bool bAsyncGetData = m_CreationArgs.SubmitBatchesToWorkerThread;
    return ImmCtx.QueryGetData(pAsync->GetImmediateNoFlush(), pData, DataSize, DoNotFlush, bAsyncGetData);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetPredication(Query* pPredicate, BOOL PredicateValue)
{
    AddToBatch(CmdSetPredication{ pPredicate, PredicateValue });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ResourceCopy(Resource* pDst, Resource* pSrc)
{
    AddToBatch(CmdResourceCopy{ pDst, pSrc });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ResourceResolveSubresource(Resource* pDst, UINT DstSubresource, Resource* pSrc, UINT SrcSubresource, DXGI_FORMAT Format)
{
    AddToBatch(CmdResolveSubresource{ pDst, pSrc, DstSubresource, SrcSubresource, Format });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ResourceCopyRegion(Resource* pDst, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, Resource* pSrc, UINT SrcSubresource, const D3D12_BOX* pSrcBox)
{
    AddToBatch(CmdResourceCopyRegion{ pDst, pSrc, DstSubresource, SrcSubresource, DstX, DstY, DstZ,
                                      pSrcBox ? *pSrcBox : m_ImmCtx.GetBoxFromResource(pSrc, SrcSubresource) });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetResourceMinLOD(Resource* pResource, FLOAT MinLOD)
{
    AddToBatch(CmdSetResourceMinLOD{ pResource, MinLOD });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::CopyStructureCount(Resource* pDstResource, UINT DstAlignedByteOffset, UAV* pSrcUAV)
{
    AddToBatch(CmdCopyStructureCount{ pDstResource, pSrcUAV, DstAlignedByteOffset });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::RotateResourceIdentities(Resource* const* ppResources, UINT Resources)
{
    AddToBatchVariableSize(CmdRotateResourceIdentities{ Resources }, Resources, ppResources);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetHardwareProtection(Resource* pResource, UINT Value)
{
    AddToBatch(CmdSetHardwareProtection{ pResource, Value });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetHardwareProtectionState(BOOL state)
{
    AddToBatch(CmdSetHardwareProtectionState{ state });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::BatchExtension(BatchedExtension* pExt, const void* pData, size_t DataSize)
{
    EmplaceInBatch<CmdExtension>(pExt, pData, DataSize);
}

BatchedContext::CmdExtension::CmdExtension(BatchedExtension* pExt, const void* pData, size_t DataSize)
    : pExt(pExt)
    , DataSize(Align(DataSize, sizeof(BatchPrimitive)))
{
    // Ensure extension data is 64-bit aligned.
    void* pDataDst = AlignPtr(this + 1);
    if (pData)
    {
        memcpy(pDataDst, pData, DataSize);
    }
}

size_t BatchedContext::CmdExtension::GetCommandSize(BatchedExtension*, const void*, size_t DataSize)
{
    return Align(sizeof(BatchedContext::CmdExtension), sizeof(BatchPrimitive)) +
        Align(DataSize, sizeof(BatchPrimitive));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ClearState()
{
    AddToBatch(CmdClearState{});
    ClearStateImpl();
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::ClearStateImpl()
{
    m_UAVs.Clear();
    m_NumScissors = 0;
    m_NumViewports = 0;
    ZeroMemory(m_Scissors, sizeof(m_Scissors));
    ZeroMemory(m_Viewports, sizeof(m_Viewports));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::UpdateTileMappings(
    Resource* pTiledResource, UINT NumTiledResourceRegions, _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords, _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
    Resource* pTilePool, UINT NumRanges, _In_reads_opt_(NumRanges) const ImmediateContext::TILE_RANGE_FLAG* pRangeFlags, _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets, _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts, ImmediateContext::TILE_MAPPING_FLAG Flags)
{
    EmplaceInBatch<CmdUpdateTileMappings>(pTiledResource, NumTiledResourceRegions, pTiledResourceRegionStartCoords, pTiledResourceRegionSizes, pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets, pRangeTileCounts, Flags);
}

BatchedContext::CmdUpdateTileMappings::CmdUpdateTileMappings(
    Resource* pTiledResource, UINT NumTiledResourceRegions, _In_reads_(NumTiledResourceRegions) const D3D12_TILED_RESOURCE_COORDINATE* pTiledResourceRegionStartCoords, _In_reads_opt_(NumTiledResourceRegions) const D3D12_TILE_REGION_SIZE* pTiledResourceRegionSizes,
    Resource* pTilePool, UINT NumRanges, _In_reads_opt_(NumRanges) const ImmediateContext::TILE_RANGE_FLAG* pRangeFlags, _In_reads_opt_(NumRanges) const UINT* pTilePoolStartOffsets, _In_reads_opt_(NumRanges) const UINT* pRangeTileCounts, ImmediateContext::TILE_MAPPING_FLAG Flags)
    : pTiledResource(pTiledResource)
    , NumTiledResourceRegions(NumTiledResourceRegions)
    , pTilePool(pTilePool)
    , NumRanges(NumRanges)
    , Flags(Flags)
    , bTiledResourceRegionSizesPresent(pTiledResourceRegionSizes != nullptr)
    , bRangeFlagsPresent(pRangeFlags != nullptr)
    , bTilePoolStartOffsetsPresent(pTilePoolStartOffsets != nullptr)
    , bRangeTileCountsPresent(pRangeTileCounts != nullptr)
{
    struct Temp { BatchedContext::CmdUpdateTileMappings Cmd; D3D12_TILED_RESOURCE_COORDINATE Coords; } *pTemp = reinterpret_cast<Temp*>(this);
    static_assert(alignof(D3D12_TILED_RESOURCE_COORDINATE) == alignof(UINT));
    static_assert(alignof(D3D12_TILE_REGION_SIZE) == alignof(UINT));

    // Note: Memory for all arrays is unconditionally allocated, even if null pointers are provided, for pointer math simplicity.
    D3D12_TILED_RESOURCE_COORDINATE* dst_pCoords = &pTemp->Coords;
    auto dst_pRegions = reinterpret_cast<D3D12_TILE_REGION_SIZE*>(dst_pCoords + NumTiledResourceRegions);
    auto dst_pRangeFlags = reinterpret_cast<ImmediateContext::TILE_RANGE_FLAG*>(dst_pRegions + NumTiledResourceRegions);
    auto dst_pTilePoolStartOffsets = reinterpret_cast<UINT*>(dst_pRangeFlags + NumRanges);
    auto dst_pRangeTileCounts = dst_pTilePoolStartOffsets + NumRanges;

    assert( Align(reinterpret_cast<size_t>(dst_pRangeTileCounts + NumRanges), sizeof(BatchPrimitive))
        == reinterpret_cast<size_t>(this)
        + GetCommandSize(pTiledResource, NumTiledResourceRegions, pTiledResourceRegionStartCoords, pTiledResourceRegionSizes,
                         pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets, pRangeTileCounts, Flags)
        );

    std::copy(pTiledResourceRegionStartCoords, pTiledResourceRegionStartCoords + NumTiledResourceRegions, dst_pCoords);
    if (bTiledResourceRegionSizesPresent)
    {
        std::copy(pTiledResourceRegionSizes, pTiledResourceRegionSizes + NumTiledResourceRegions, dst_pRegions);
    }
    if (bRangeFlagsPresent)
    {
        std::copy(pRangeFlags, pRangeFlags + NumRanges, dst_pRangeFlags);
    }
    if (bTilePoolStartOffsetsPresent)
    {
        std::copy(pTilePoolStartOffsets, pTilePoolStartOffsets + NumRanges, dst_pTilePoolStartOffsets);
    }
    if (bRangeTileCountsPresent)
    {
        std::copy(pRangeTileCounts, pRangeTileCounts + NumRanges, dst_pRangeTileCounts);
    }
}

size_t BatchedContext::CmdUpdateTileMappings::GetCommandSize(
    Resource*, UINT NumTiledResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*,
    Resource*, UINT NumRanges, const ImmediateContext::TILE_RANGE_FLAG*,
    const UINT*, const UINT*, ImmediateContext::TILE_MAPPING_FLAG)
{
    struct Temp { BatchedContext::CmdUpdateTileMappings Cmd; D3D12_TILED_RESOURCE_COORDINATE Coords; };
    // Note: Memory for all arrays is unconditionally allocated, even if null pointers are provided, for pointer math simplicity.
    return Align(offsetof(Temp, Coords) +
        (sizeof(D3D12_TILED_RESOURCE_COORDINATE) + sizeof(D3D12_TILE_REGION_SIZE)) * NumTiledResourceRegions +
        (sizeof(ImmediateContext::TILE_RANGE_FLAG) + sizeof(UINT) * 2) * NumRanges,
                 sizeof(BatchPrimitive));
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::CopyTileMappings(Resource* pDstTiledResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstStartCoords, Resource* pSrcTiledResource, _In_ const  D3D12_TILED_RESOURCE_COORDINATE* pSrcStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, ImmediateContext::TILE_MAPPING_FLAG Flags)
{
    AddToBatch(CmdCopyTileMappings{ pDstTiledResource, pSrcTiledResource, *pDstStartCoords, *pSrcStartCoords, *pTileRegion, Flags });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::CopyTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pStartCoords, _In_ const D3D12_TILE_REGION_SIZE* pTileRegion, Resource* pBuffer, UINT64 BufferOffset, ImmediateContext::TILE_COPY_FLAG Flags)
{
    AddToBatch(CmdCopyTiles{ pResource, pBuffer, *pStartCoords, *pTileRegion, BufferOffset, Flags });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::UpdateTiles(Resource* pResource, _In_ const D3D12_TILED_RESOURCE_COORDINATE* pCoord, _In_ const D3D12_TILE_REGION_SIZE* pRegion, const _In_ VOID* pData, UINT Flags)
{
    UINT DataSize = pRegion->NumTiles * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;

    ResourceCreationArgs ResourceArgs = {};
    ResourceArgs.m_appDesc = AppResourceDesc(1, 1, 1, 1, 1, 1, DataSize, 1, DXGI_FORMAT_UNKNOWN, 1, 0, RESOURCE_USAGE_STAGING, RESOURCE_CPU_ACCESS_WRITE, (RESOURCE_BIND_FLAGS)0, D3D12_RESOURCE_DIMENSION_BUFFER);
    ResourceArgs.m_heapDesc = CD3DX12_HEAP_DESC(DataSize, GetImmediateContextNoFlush().GetHeapProperties(D3D12_HEAP_TYPE_UPLOAD));
    ResourceArgs.m_desc12 = CD3DX12_RESOURCE_DESC::Buffer(DataSize);
    unique_comptr<Resource> spResource = Resource::CreateResource(&GetImmediateContextNoFlush(), ResourceArgs, ResourceAllocationContext::FreeThread); // throws

    MappedSubresource Mapped;
    GetImmediateContextNoFlush().MapUnderlying(spResource.get(), 0, MAP_TYPE_WRITE, nullptr, &Mapped);

    memcpy(Mapped.pData, pData, DataSize);

    CD3DX12_RANGE WrittenRange(0, DataSize);
    GetImmediateContextNoFlush().UnmapUnderlyingStaging(spResource.get(), 0, nullptr);

    CopyTiles(pResource, pCoord, pRegion, spResource.get(), 0, (ImmediateContext::TILE_COPY_FLAG)(Flags | ImmediateContext::TILE_COPY_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE));
    ReleaseResource(spResource.release());
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::TiledResourceBarrier(Resource* pBefore, Resource* pAfter)
{
    AddToBatch(CmdTiledResourceBarrier{ pBefore, pAfter });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::ResizeTilePool(Resource* pResource, UINT64 NewSize)
{
    AddToBatch(CmdResizeTilePool{ pResource, NewSize });
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SetMarker(const wchar_t* name)
{
    UINT NumChars = (UINT)wcslen(name) + 1;
    AddToBatchVariableSize(CmdSetMarker{ NumChars }, NumChars, name);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::BeginEvent(const wchar_t* name)
{
    UINT NumChars = (UINT)wcslen(name) + 1;
    AddToBatchVariableSize(CmdBeginEvent{ NumChars }, NumChars, name);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::EndEvent()
{
    AddToBatch(CmdEndEvent{ });
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::ProcessBatchWork(BatchStorage& batch)
{
    // Ensure the batch is cleared, even if a batch function errors out
    auto FunctionExit = MakeScopeExit([&batch]()
    {
        batch.clear();
    });

    for (auto segmentIter = batch.segments_begin();
         segmentIter != batch.segments_end();
         ++segmentIter)
    {
        const void* pCommandData = segmentIter->begin();
        while (pCommandData < segmentIter->end())
        {
            UINT const& CmdValue = *reinterpret_cast<UINT const*>(pCommandData);
            ASSUME(CmdValue <= c_LastCommand);
            m_DispatchTable[CmdValue](m_ImmCtx, pCommandData); // throws
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API BatchedContext::ProcessBatch()
{
    assert(!IsBatchThread());
    assert(m_CreationArgs.pParentContext == nullptr);
    if (m_CreationArgs.SubmitBatchesToWorkerThread)
    {
        auto Lock = m_RecordingLock.TakeLock();
        SubmitBatch();
        return WaitForBatchThreadIdle();
    }
    else
    {
        // Ensure destroys are executed even if a batch function errors out.
        auto Lock = m_RecordingLock.TakeLock();
        auto FunctionExit = MakeScopeExit([this]()
        {
            for (auto& fn : m_PostBatchFunctions)
            {
                fn();
            }
            m_PostBatchFunctions.clear();

            m_CurrentCommandCount = 0;
            m_PendingDestructionMemorySize = 0;
            m_CompletedBatchID = m_RecordingBatchID;

            ++m_RecordingBatchID;
            if (static_cast<uint32_t>(m_RecordingBatchID) == 0)
            {
                // Rolled into the next "generation", but each generation should start with ID 1
                ++m_RecordingBatchID;
            }
        });

        bool bRet = !m_CurrentBatch.empty() || !m_PostBatchFunctions.empty();
        ProcessBatchWork(m_CurrentBatch); // throws
        return bRet;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
std::unique_ptr<BatchedContext::Batch> BatchedContext::GetIdleBatch()
{
    // Assumed m_SubmissionLock is held.

    // Find a free batch if possible
    if (!m_FreeBatches.empty())
    {
        auto batch = std::move(m_FreeBatches.front());
        m_FreeBatches.pop_front();
        return batch;
    }
    return std::unique_ptr<Batch>(new Batch(m_BatchStorageAllocator));
}

//----------------------------------------------------------------------------------------------------------------------------------
std::unique_ptr<BatchedContext::Batch> BatchedContext::FinishBatch(bool bFlushImmCtxAfterBatch)
{
    assert(!IsBatchThread());
    BatchStorage NewBatch(m_BatchStorageAllocator);
    std::vector<std::function<void()>> NewPostBatchFunctions;
    std::unique_ptr<Batch> pRet;

    // Synchronize with threads recording to the batch
    {
        auto Lock = m_RecordingLock.TakeLock();
        if (m_CurrentBatch.empty() && m_PostBatchFunctions.empty())
        {
            return nullptr;
        }
        std::swap(m_CurrentBatch, NewBatch);
        std::swap(m_PostBatchFunctions, NewPostBatchFunctions);

        // Synchronize with the worker thread potentially retiring batches
        {
            auto SubmissionLock = m_SubmissionLock.TakeLock();
            pRet = GetIdleBatch();
        }

        pRet->PrepareToSubmit(std::move(NewBatch), std::move(NewPostBatchFunctions), m_RecordingBatchID, m_CurrentCommandCount, bFlushImmCtxAfterBatch);
        m_CurrentCommandCount = 0;
        m_PendingDestructionMemorySize = 0;

        ++m_RecordingBatchID;
        if (static_cast<uint32_t>(m_RecordingBatchID) == 0)
        {
            // Rolled into the next "generation", but each generation should start with ID 1
            ++m_RecordingBatchID;
        }
    }

    return std::move(pRet);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SubmitCommandListBatch(Batch* pBatch)
{
    assert(!IsBatchThread());
    auto pExecutionContext = m_CreationArgs.pParentContext ? m_CreationArgs.pParentContext : this;

    auto Lock = m_RecordingLock.TakeLock();
    AddToBatch(m_CurrentBatch, CmdExecuteNestedBatch{ pBatch, pExecutionContext });

    m_CurrentCommandCount += pBatch->m_NumCommands;

    // Increase the batch ID to the next "generation"
    // See the comments in SyncWithBatch for more details
    constexpr uint64_t GenerationIDMask = 0xffffffff00000000ull;
    m_RecordingBatchID = (m_RecordingBatchID & GenerationIDMask) + (1ull << 32ull) + 1;

    SubmitBatchIfIdle(pBatch->m_NumCommands >= c_CommandKickoffMinThreshold);

    // It's guaranteed that a command list both begins and ends with a ClearState command, so we'll
    // clear our own tracked state here.
    ClearStateImpl();
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::RetireBatch(std::unique_ptr<Batch> pBatch)
{
    // Note: Only used for command list batches - implicit batches have slightly different retiring semantics
    for (auto& fn : pBatch->m_PostBatchFunctions)
    {
        fn();
    }

    auto Lock = m_SubmissionLock.TakeLock();

    pBatch->Retire(m_FreePages);
    m_FreeBatches.emplace_back(std::move(pBatch));
}

//----------------------------------------------------------------------------------------------------------------------------------
bool TRANSLATION_API BatchedContext::SubmitBatch(bool bFlushImmCtxAfterBatch)
{
    assert(!IsBatchThread());
    assert(m_CreationArgs.pParentContext == nullptr);

    if (!m_CreationArgs.SubmitBatchesToWorkerThread)
    {
        return ProcessBatch();
    }

    auto pBatch = FinishBatch(bFlushImmCtxAfterBatch);
    if (!pBatch)
    {
        return false;
    }

    {
        auto Lock = m_SubmissionLock.TakeLock();
        m_QueuedBatches.emplace_back(std::move(pBatch));
    }

    {
        auto Lock = m_RecordingLock.TakeLock();

        // Check if there's room in the semaphores.
        assert(m_NumOutstandingBatches <= c_MaxOutstandingBatches);
        if (m_NumOutstandingBatches == c_MaxOutstandingBatches)
        {
            WaitForSingleBatch(INFINITE);
            assert(m_NumOutstandingBatches < c_MaxOutstandingBatches);
        }

        // Wake up the batch thread
        BOOL value = ReleaseSemaphore(m_BatchSubmittedSemaphore, 1, nullptr);
        assert(value == TRUE);
        UNREFERENCED_PARAMETER(value);

        ++m_NumOutstandingBatches;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::SubmitBatchIfIdle(bool bSkipFrequencyCheck)
{
    assert(!IsBatchThread());
    assert(m_CurrentCommandCount > 0);
    if (m_CreationArgs.SubmitBatchesToWorkerThread && // Don't do work on the app thread.
        (bSkipFrequencyCheck ||
            m_CurrentCommandCount % c_CommandKickoffMinThreshold == 0) && // Avoid checking for idle all the time, it's not free.
        IsBatchThreadIdle())
    {
        SubmitBatch();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
bool BatchedContext::WaitForBatchThreadIdle()
{
    assert(!IsBatchThread());
    bool bRet = false;
    while (m_NumOutstandingBatches)
    {
        bRet = true;
        WaitForSingleBatch(INFINITE);
    }
    return bRet;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool BatchedContext::IsBatchThreadIdle()
{
    assert(!IsBatchThread());
    while (m_NumOutstandingBatches > 0 && WaitForSingleBatch(0));
    return m_NumOutstandingBatches == 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
bool BatchedContext::WaitForSingleBatch(DWORD timeout)
{
    assert(!IsBatchThread());
    if (WaitForSingleObject(m_BatchConsumedSemaphore, timeout) == WAIT_OBJECT_0)
    {
        --m_NumOutstandingBatches;
        if (m_bFlushPendingCallback.exchange(false))
        {
            m_Callbacks.PostSubmitCallback();
        }
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::ProcessBatchImpl(Batch* pBatchToProcess)
{
    try
    {
        ProcessBatchWork(pBatchToProcess->m_BatchCommands); // throws
    }
    catch (_com_error& hrEx) { m_Callbacks.ThreadErrorCallback(hrEx.Error()); }
    catch (std::bad_alloc&) { m_Callbacks.ThreadErrorCallback(E_OUTOFMEMORY); }
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::BatchThread()
{
    assert(IsBatchThread());

    HMODULE hMyModule; // Keep this module loaded until this thread safely returns.
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&c_LastCommand), &hMyModule);
    auto ScopeExit = MakeScopeExit([hMyModule]()
    {
        FreeLibrary(hMyModule);
    });
    while (true)
    {
        // Wait for work
        WaitForSingleObject(m_BatchSubmittedSemaphore, INFINITE);

        // Figure out what we're supposed to be working on
        Batch* pBatchToProcess = nullptr;
        {
            auto Lock = m_SubmissionLock.TakeLock();
            if (!m_QueuedBatches.empty())
            {
                pBatchToProcess = m_QueuedBatches.front().get();
            }
        }

        // Semaphore was signaled but there's no work to be done, exit thread.
        if (!pBatchToProcess)
        {
            return;
        }

        // Do the work
        ProcessBatchImpl(pBatchToProcess);

        // Retire the batch
        for (auto& fn : pBatchToProcess->m_PostBatchFunctions)
        {
            fn();
        }

        {
            auto Lock = m_SubmissionLock.TakeLock();

            UINT FlushRequestedMask = pBatchToProcess->m_FlushRequestedMask;
            m_CompletedBatchID = pBatchToProcess->m_BatchID;

            pBatchToProcess->Retire(m_FreePages);
            m_FreeBatches.emplace_back(std::move(m_QueuedBatches.front()));
            m_QueuedBatches.pop_front();

            m_ImmCtx.Flush(FlushRequestedMask);
        }

        BOOL value = ReleaseSemaphore(m_BatchConsumedSemaphore, 1, nullptr);
        assert(value == TRUE);
        UNREFERENCED_PARAMETER(value);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
bool BatchedContext::IsBatchThread()
{
    return m_CreationArgs.SubmitBatchesToWorkerThread &&
        GetCurrentThreadId() == GetThreadId(m_BatchThread.m_h);
}

//----------------------------------------------------------------------------------------------------------------------------------
void TRANSLATION_API BatchedContext::PostSubmit()
{
    if (IsBatchThread())
    {
        // If we're on the worker thread, just indicate that we've flushed.
        // The app thread will consume this information when draining the work queue.
        m_bFlushPendingCallback = true;
    }
    else
    {
        // If we're on the app thread, call into the next layer up.
        m_Callbacks.PostSubmitCallback();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void* FreePageContainer::RemovePage() noexcept
{
    auto Lock = m_CS.TakeLock();
    if (m_FreePageHead != nullptr)
    {
        void* pPage = m_FreePageHead;
        m_FreePageHead = *reinterpret_cast<void**>(pPage);
        return pPage;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------------------
FreePageContainer::~FreePageContainer()
{
    while (void* pPage = RemovePage())
    {
        operator delete(pPage);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
void* BatchedContext::BatchStorageAllocator::operator()(bool bAllocSuccess) noexcept
{
    if (bAllocSuccess && m_Container)
    {
        return m_Container->RemovePage();
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------------------
void FreePageContainer::LockedAdder::AddPage(void* pPage) noexcept
{
    *reinterpret_cast<void**>(pPage) = m_FreePageHead;
    m_FreePageHead = pPage;
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::Batch::Retire(FreePageContainer& FreePages) noexcept
{
    FreePageContainer::LockedAdder Adder(FreePages);
    for (auto& segment : m_BatchCommands.m_segments)
    {
        Adder.AddPage(segment.begin());
    }
    m_BatchCommands.m_segments.clear();
    m_PostBatchFunctions.clear();
}

//----------------------------------------------------------------------------------------------------------------------------------
void BatchedContext::Batch::PrepareToSubmit(BatchStorage BatchCommands, std::vector<std::function<void()>> PostBatchFunctions, uint64_t BatchID, UINT NumCommands, bool bFlushImmCtxAfterBatch)
{
    m_BatchCommands = std::move(BatchCommands);
    m_PostBatchFunctions = std::move(PostBatchFunctions);
    m_BatchID = BatchID;
    m_NumCommands = NumCommands;
    m_FlushRequestedMask = bFlushImmCtxAfterBatch ? COMMAND_LIST_TYPE_ALL_MASK : 0;
}

}
