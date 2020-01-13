// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once
namespace D3D12TranslationLayer
{

    //----------------------------------------------------------------------------------------------------------------------------------
    inline Sampler::Sampler(ImmediateContext* pDevice, D3D12_SAMPLER_DESC const& desc) noexcept(false)
        : DeviceChild(pDevice)
    {
        if (!pDevice->ComputeOnly())
        {
            m_Descriptor = pDevice->m_SamplerAllocator.AllocateHeapSlot(&m_DescriptorHeapIndex); // throw( _com_error )
            pDevice->m_pDevice12->CreateSampler(&desc, m_Descriptor);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    inline Sampler::~Sampler() noexcept
    {
        if (!m_pParent->ComputeOnly())
        {
            m_pParent->m_SamplerAllocator.FreeHeapSlot(m_Descriptor, m_DescriptorHeapIndex);
        }
    }
};