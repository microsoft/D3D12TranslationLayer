// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <numeric>

namespace D3D12TranslationLayer
{
    void RootSignatureBase::Create(D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& rootDesc) noexcept(false)
    {
        CComPtr<ID3DBlob> spBlob;

        ThrowFailure(D3D12SerializeVersionedRootSignature(&rootDesc, &spBlob, NULL));

        Create(spBlob->GetBufferPointer(), spBlob->GetBufferSize());
    }
    void RootSignatureBase::Create(const void* pBlob, SIZE_T BlobSize) noexcept(false)
    {
        Destroy();

        ThrowFailure(m_pParent->m_pDevice12->CreateRootSignature(m_pParent->GetNodeMask(), pBlob, BlobSize, IID_PPV_ARGS(GetForCreate())));
    }

    D3D12_SHADER_VISIBILITY GetShaderVisibility(EShaderStage stage)
    {
        switch (stage)
        {
        case e_VS: return D3D12_SHADER_VISIBILITY_VERTEX;
        case e_PS: return D3D12_SHADER_VISIBILITY_PIXEL;
        case e_GS: return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case e_HS: return D3D12_SHADER_VISIBILITY_HULL;
        case e_DS: return D3D12_SHADER_VISIBILITY_DOMAIN;
        default: return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    void RootSignatureDesc::GetAsD3D12Desc(VersionedRootSignatureDescWithStorage& Storage, ImmediateContext* pParent) const
    {
        const bool bGraphics = (m_Flags & Compute) == 0;
        constexpr UINT MaxShaderStages = std::extent<decltype(m_ShaderStages)>::value;
        const UINT NumShaderStages = bGraphics ? MaxShaderStages : 1;

        UINT RangeIndex = 0;
        UINT ParameterIndex = 0;
        bool bCB14 = false;
        D3D12_DESCRIPTOR_RANGE_FLAGS RangeFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        if (m_Flags & RequiresBufferOutOfBoundsHandling)
        {
            RangeFlags |= D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;
        }
        // else STATIC (default)
        static constexpr D3D12_DESCRIPTOR_RANGE_FLAGS InterfacesRangeFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        UINT NumSRVRanges = std::accumulate(m_NumSRVSpacesUsed, std::end(m_NumSRVSpacesUsed), 0);
        CD3DX12_DESCRIPTOR_RANGE1* pSRVRanges;
        if (NumSRVRanges > MaxShaderStages)
        {
            Storage.InterfacesSRVRanges.resize(NumSRVRanges);
            pSRVRanges = Storage.InterfacesSRVRanges.data();
        }
        else
        {
            pSRVRanges = std::end(Storage.DescriptorRanges) - NumSRVRanges;
        }

        for (UINT i = 0; i < NumShaderStages; ++i)
        {
            ASSUME(bGraphics || i == 0);
            EShaderStage StageEnum = bGraphics ? (EShaderStage)i : e_CS;
            auto Visibility = GetShaderVisibility(StageEnum);
            ShaderStage const& Stage = m_ShaderStages[i];

            bCB14 |= Stage.IsCB14();
            Storage.Parameter[ParameterIndex].InitAsDescriptorTable(1, &Storage.DescriptorRanges[RangeIndex], Visibility);
            Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, Stage.GetCBBindingCount(), 0, 0, RangeFlags);
            if (Stage.m_UsesShaderInterfaces)
            {
                Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, 0, 1, InterfacesRangeFlags, 0);
                Storage.Parameter[ParameterIndex].DescriptorTable.NumDescriptorRanges++;
            }
            ++ParameterIndex;
            Storage.Parameter[ParameterIndex].InitAsDescriptorTable(m_NumSRVSpacesUsed[i], pSRVRanges, Visibility);
            for (UINT range = 0; range < m_NumSRVSpacesUsed[i]; ++range)
            {
                pSRVRanges->Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, Stage.GetSRVBindingCount(), 0, range, range == 0 ? RangeFlags : InterfacesRangeFlags, 0);
                ++pSRVRanges;
            }
            ++ParameterIndex;
            if (pParent->ComputeOnly())
            {
                // Dummy descriptor range just to make the root parameter constants line up
                Storage.Parameter[ParameterIndex].InitAsDescriptorTable(1, &Storage.DescriptorRanges[RangeIndex], Visibility);
                Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, m_NumSRVSpacesUsed[i] + 1, RangeFlags);
            }
            else
            {
                Storage.Parameter[ParameterIndex].InitAsDescriptorTable(1, &Storage.DescriptorRanges[RangeIndex], Visibility);
                Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, Stage.GetSamplerBindingCount(), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                if (Stage.m_UsesShaderInterfaces)
                {
                    Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);
                    Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT, 0, 2, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);
                    Storage.Parameter[ParameterIndex].DescriptorTable.NumDescriptorRanges += 2;
                }
            }
            ++ParameterIndex;
        }
        Storage.Parameter[ParameterIndex].InitAsDescriptorTable(1, &Storage.DescriptorRanges[RangeIndex], D3D12_SHADER_VISIBILITY_ALL);
        Storage.DescriptorRanges[RangeIndex++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, GetUAVBindingCount(), 0, 0, RangeFlags);
        ++ParameterIndex;

        assert(ParameterIndex <= VersionedRootSignatureDescWithStorage::c_NumParameters);
        assert(RangeIndex <= VersionedRootSignatureDescWithStorage::c_NumParameters + VersionedRootSignatureDescWithStorage::c_NumExtraRangesForInterfaces);

        const D3D12_ROOT_SIGNATURE_FLAGS BaseFlags =
            !bGraphics ? D3D12_ROOT_SIGNATURE_FLAG_NONE :
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

        D3D12_ROOT_SIGNATURE_FLAGS Flags = BaseFlags |
            (bCB14 ? ROOT_SIGNATURE_FLAG_ALLOW_LOW_TIER_RESERVED_HW_CB_LIMIT : D3D12_ROOT_SIGNATURE_FLAG_NONE);
        Storage.RootDesc.Init_1_1(ParameterIndex, Storage.Parameter, 0, NULL, Flags);
    }
};