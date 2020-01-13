// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    //==================================================================================================================================
    // Sampler
    // Stores data responsible for remapping D3D11 samplers to underlying D3D12 samplers
    //==================================================================================================================================
    class Sampler : public DeviceChild
    {
    public:
        Sampler(ImmediateContext* pDevice, D3D12_SAMPLER_DESC const& desc) noexcept(false);
        ~Sampler() noexcept;

    public:
        D3D12_CPU_DESCRIPTOR_HANDLE m_Descriptor;
        UINT m_DescriptorHeapIndex;
    };
};