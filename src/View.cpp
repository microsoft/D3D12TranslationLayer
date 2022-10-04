// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{

    void ViewBase::UsedInCommandList(COMMAND_LIST_TYPE commandListType, UINT64 id) 
    {
        if (m_pResource) { m_pResource->UsedInCommandList(commandListType, id); }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UAV::UAV(ImmediateContext* pDevice, const TTranslationLayerDesc &Desc, Resource &ViewResource) noexcept(false)
        : TUAV(pDevice, Desc.m_Desc12, ViewResource), m_D3D11UAVFlags(Desc.m_D3D11UAVFlags)
    {
    }

    UAV::~UAV() noexcept(false)
    {
        AddToDeferredDeletionQueue(m_pCounterResource);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void UAV::EnsureCounterResource() noexcept(false)
    {
        // This is called by an immediate context operation
        // to ensure that access to the command list is synchronized
        const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc12 = GetDesc12();

        // If either the Append or Counter bits are set and the counter resource has not been created
        // then create a resource to hold the count
        const UINT CounterFlags = D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER;

        if (!m_pCounterResource &&
            ((D3D12_UAV_DIMENSION_BUFFER == Desc12.ViewDimension) &&
            (m_D3D11UAVFlags & CounterFlags)))
        {
            unique_comptr<ID3D12Resource> pCounterResource;

            // Use a readback heap on drivers that don't support the heap DDIs
            // becuase that is the only way to create a resource in the COPY_DEST state
            D3D12_HEAP_PROPERTIES HeapProp = m_pParent->GetHeapProperties(
                D3D12_HEAP_TYPE_DEFAULT
                );

            D3D12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
                sizeof(UINT),
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                );

            HRESULT hr = m_pParent->m_pDevice12->CreateCommittedResource(
                &HeapProp,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&pCounterResource)
                );

            if (FAILED(hr))
            {
                ThrowFailure(hr);
            }

            // Initialize the counter to 0
            UINT InitialData = 0;

            UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, m_pParent->GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
            m_pParent->CopyDataToBuffer(
                pCounterResource.get(),
                0,
                &InitialData,
                sizeof(InitialData)
                ); // throw( _com_error )

            // No more failures after this point
            m_pCounterResource = std::move(pCounterResource);

            // Transition the counter to the UAV state
            D3D12_RESOURCE_BARRIER BarrierDesc;
            ZeroMemory(&BarrierDesc, sizeof(BarrierDesc));

            BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            BarrierDesc.Transition.pResource = m_pCounterResource.get();
            BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

            m_pParent->GetGraphicsCommandList()->ResourceBarrier(1, &BarrierDesc);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void UAV::UpdateCounterValue(UINT Value)
    {
        // Early-out if this is not a counter UAV
        if (!m_pCounterResource)
        {
            return;
        }

        // Transition the counter to the CopyDest state
        AutoTransition AutoTransition(
            m_pParent->GetGraphicsCommandList(),
            m_pCounterResource.get(),
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST
            );
        UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, m_pParent->GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));

        // UpdateSubresource
        m_pParent->CopyDataToBuffer(
            m_pCounterResource.get(),
            0,
            &Value,
            sizeof(Value)
            ); // throw( _com_error )
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void UAV::CopyCounterToBuffer(ID3D12Resource* pDst, UINT DstOffset) noexcept
    {
        // Early-out if there was a previous error which caused the counter resource to not be allocated
        if (!m_pCounterResource)
        {
            return;
        }

        // Transition the counter to the CopySource state (and put it back at the end of this function)
        AutoTransition AutoTransition(
            m_pParent->GetGraphicsCommandList(),
            m_pCounterResource.get(),
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
            );

        UsedInCommandList(COMMAND_LIST_TYPE::GRAPHICS, m_pParent->GetCommandListID(COMMAND_LIST_TYPE::GRAPHICS));
        m_pParent->GetGraphicsCommandList()->CopyBufferRegion(
            pDst,
            DstOffset,
            m_pCounterResource.get(),
            0,
            sizeof(UINT)
            );
    }
};