// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include <array>

namespace D3D12TranslationLayer
{
#define ROOT_SIGNATURE_FLAG_ALLOW_LOW_TIER_RESERVED_HW_CB_LIMIT ((D3D12_ROOT_SIGNATURE_FLAGS)0x80000000)

    struct VersionedRootSignatureDescWithStorage
    {
        static constexpr UINT c_NumParameters = 16; // (CBV, SRV, Sampler) * 5 shader stages + UAV
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootDesc{ 0, (D3D12_ROOT_PARAMETER1*)nullptr };
        CD3DX12_ROOT_PARAMETER1 Parameter[c_NumParameters];
        static constexpr UINT c_NumExtraRangesForInterfaces = 15; // (CBs in space 1, samplers in spaces 1 and 2) * 5 shader stages
        CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[c_NumParameters + c_NumExtraRangesForInterfaces];

        std::vector<CD3DX12_DESCRIPTOR_RANGE1> InterfacesSRVRanges;

        VersionedRootSignatureDescWithStorage() = default;
        // Noncopyable, non-movable due to internal pointers.
        VersionedRootSignatureDescWithStorage(VersionedRootSignatureDescWithStorage const&) = delete;
        VersionedRootSignatureDescWithStorage(VersionedRootSignatureDescWithStorage&&) = delete;
        VersionedRootSignatureDescWithStorage& operator=(VersionedRootSignatureDescWithStorage const&) = delete;
        VersionedRootSignatureDescWithStorage& operator=(VersionedRootSignatureDescWithStorage&&) = delete;
    };

    struct RootSignatureDesc
    {
        static_assert(D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT == 14, "Validation constant.");
        static_assert(D3D12_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT == 15, "Validation constant.");
        static constexpr UINT8 c_CBBuckets = 4; // 4, 8, 14, 15
        static_assert(D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT == 128, "Validating constant.");
        static constexpr UINT8 c_SRVBuckets = 6; // 4, 8, 16, 32, 64, 128
        static_assert(D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT == 16, "Validating constant.");
        static constexpr UINT8 c_SamplerBuckets = 3; // 4, 8, 16
        static_assert(D3D12_UAV_SLOT_COUNT == 64, "Validating constant.");
        static constexpr UINT8 c_UAVBuckets = 5; // 4, 8, 16, 32, 64

        static UINT8 NonCBBindingCountToBucket(UINT BindingCount)
        {
            if (BindingCount <= 4)
            {
                return 0;
            }
            DWORD Bucket = 0;
            BitScanReverse(&Bucket, BindingCount - 1);
            // Bucket guaranteed to be at least 2 due to <= 4 check above.
            return static_cast<UINT8>(Bucket - 1);
        }
        static UINT8 CBBindingCountToBucket(UINT BindingCount)
        {
            if (BindingCount == 15) return 3;
            else if (BindingCount > 8) return 2;
            else if (BindingCount > 4) return 1;
            return 0;
        }
        static constexpr UINT NonCBBucketToBindingCount(UINT8 Bucket)
        {
            return 4 << Bucket;
        }
        static UINT CBBucketToBindingCount(UINT8 Bucket)
        {
            static constexpr UINT BindingCounts[c_CBBuckets] = { 4, 8, 14, 15 };
            return BindingCounts[Bucket];
        }

        struct ShaderStage
        {
            UINT8 m_CBBucket : 2;
            UINT8 m_SamplerBucket : 2;
            UINT8 m_SRVBucket : 3;
            UINT8 m_UsesShaderInterfaces : 1;

            ShaderStage()
            {
                m_CBBucket = 2;
                m_SamplerBucket = c_SamplerBuckets - 1;
                m_SRVBucket = c_SRVBuckets - 1;
                m_UsesShaderInterfaces = 0;
            }
            ShaderStage(SShaderDecls const* pShader) : ShaderStage()
            {
                if (pShader)
                {
                    m_CBBucket = CBBindingCountToBucket(pShader->m_NumCBs);
                    m_SamplerBucket = NonCBBindingCountToBucket(pShader->m_NumSamplers);
                    m_SRVBucket = NonCBBindingCountToBucket((UINT)pShader->m_ResourceDecls.size());
                    m_UsesShaderInterfaces = pShader->m_bUsesInterfaces;
                }
            }
            ShaderStage(ShaderStage const&) = default;
            ShaderStage& operator=(ShaderStage const&) = default;
            UINT GetCBBindingCount() const { return CBBucketToBindingCount(m_CBBucket); }
            UINT GetSamplerBindingCount() const { return NonCBBucketToBindingCount(m_SamplerBucket); }
            UINT GetSRVBindingCount() const { return NonCBBucketToBindingCount(m_SRVBucket); }
            bool IsCB14() const { return m_CBBucket == 3; }
        };

        ShaderStage m_ShaderStages[5]; // Only graphics stages are needed
        UINT8 m_UAVBucket;
        enum Flags : UINT16
        {
            Compute = 1,
            RequiresBufferOutOfBoundsHandling = 2,
            UsesShaderInterfaces = 4,
        };
        const Flags m_Flags;

        UINT m_NumSRVSpacesUsed[5];

        template <int N>
        static Flags ComputeFlags(bool bRequiresBufferOutOfBoundsHandling, std::array<SShaderDecls const*, N> const& shaders)
        {
            UINT flags =
                ((N == 1) ? Compute : 0) |
                (bRequiresBufferOutOfBoundsHandling ? RequiresBufferOutOfBoundsHandling : 0);
            for (SShaderDecls const* pShader : shaders)
            {
                if (pShader && pShader->m_bUsesInterfaces)
                {
                    flags |= UsesShaderInterfaces;
                    break;
                }
            }
            return (Flags)flags;
        }

        RootSignatureDesc(SShaderDecls const* pVS, SShaderDecls const* pPS, SShaderDecls const* pGS, SShaderDecls const* pHS, SShaderDecls const* pDS, bool bRequiresBufferOutOfBoundsHandling)
            : m_ShaderStages{
                { ShaderStage(pPS) },
                { ShaderStage(pVS) },
                { ShaderStage(pGS) },
                { ShaderStage(pHS) },
                { ShaderStage(pDS) } }
            , m_UAVBucket(NonCBBindingCountToBucket(NumUAVBindings(pVS, pPS, pGS, pHS, pDS)))
            , m_Flags(ComputeFlags<5>(bRequiresBufferOutOfBoundsHandling, std::array<SShaderDecls const*, 5>{ pPS, pVS, pGS, pHS, pDS }))
            , m_NumSRVSpacesUsed{
                pPS ? pPS->m_NumSRVSpacesUsed : 1u,
                pVS ? pVS->m_NumSRVSpacesUsed : 1u,
                pGS ? pGS->m_NumSRVSpacesUsed : 1u,
                pHS ? pHS->m_NumSRVSpacesUsed : 1u,
                pDS ? pDS->m_NumSRVSpacesUsed : 1u }
        {
        }
        RootSignatureDesc(SShaderDecls const* pCS, bool bRequiresBufferOutOfBoundsHandling)
            : m_ShaderStages{
                { ShaderStage(pCS) } }
                , m_UAVBucket(NonCBBindingCountToBucket(pCS ? (UINT)pCS->m_UAVDecls.size() : 0u))
            , m_Flags(ComputeFlags<1>(bRequiresBufferOutOfBoundsHandling, std::array<SShaderDecls const*, 1>{ pCS }))
            , m_NumSRVSpacesUsed{ pCS ? pCS->m_NumSRVSpacesUsed : 1u }
        {
        }
        RootSignatureDesc(RootSignatureDesc const&) = default;
        RootSignatureDesc& operator=(RootSignatureDesc const&) = default;

        UINT GetUAVBindingCount() const { return NonCBBucketToBindingCount(m_UAVBucket); }
        UINT64 GetAsUINT64() const { return *reinterpret_cast<const UINT64*>(this); }
        void GetAsD3D12Desc(VersionedRootSignatureDescWithStorage& Storage, ImmediateContext* pParent) const;

        bool operator==(RootSignatureDesc const& o) const
        {
            if (GetAsUINT64() != o.GetAsUINT64()) // Check non-interface equality
                return false;
            if (!(m_Flags & UsesShaderInterfaces)) // If no interfaces, then equal
                return true;
            // Check additional interface-specific data
            return memcmp(m_NumSRVSpacesUsed, o.m_NumSRVSpacesUsed, sizeof(m_NumSRVSpacesUsed)) == 0;
        }

        template <EShaderStage s> ShaderStage const& GetShaderStage() const
        {
            static_assert(s < ShaderStageCount);
            if constexpr (s == e_CS)
            {
                return m_ShaderStages[0];
            }
            else
            {
                return m_ShaderStages[(UINT)s];
            }
        }

    private:
        static UINT NumUAVBindings(SShaderDecls const* pVS, SShaderDecls const* pPS, SShaderDecls const* pGS, SShaderDecls const* pHS, SShaderDecls const* pDS)
        {
            UINT MaxUAVCount = 0;
            SShaderDecls const* arr[] = { pVS, pPS, pGS, pHS, pDS };
            for (auto p : arr)
            {
                if (p && p->m_UAVDecls.size() > MaxUAVCount)
                {
                    MaxUAVCount = (UINT)p->m_UAVDecls.size();
                }
            }
            return MaxUAVCount;
        }
    };

    static_assert(offsetof(RootSignatureDesc, m_NumSRVSpacesUsed) == sizeof(UINT64), "Using as key");

    class RootSignatureBase : protected DeviceChildImpl<ID3D12RootSignature>
    {
    public:
        RootSignatureBase(ImmediateContext* pParent)
            : DeviceChildImpl(pParent)
        {
        }

    protected:
        void Create(D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& rootDesc) noexcept(false);
        void Create(const void* pBlob, SIZE_T BlobSize) noexcept(false);
    };

    class InternalRootSignature : public RootSignatureBase
    {
    public:
        InternalRootSignature(ImmediateContext* pParent)
            : RootSignatureBase(pParent)
        {
        }

        using RootSignatureBase::Create;
        using DeviceChildImpl::Created;
        ID3D12RootSignature* GetRootSignature() { return GetForUse(COMMAND_LIST_TYPE::GRAPHICS); }
    };

    class RootSignature : public RootSignatureBase
    {
    public:
        RootSignature(ImmediateContext* pParent, RootSignatureDesc const& desc)
            : RootSignatureBase(pParent)
            , m_Desc(desc)
        {
            VersionedRootSignatureDescWithStorage Storage;
            m_Desc.GetAsD3D12Desc(Storage, pParent);
            Create(Storage.RootDesc);
        }

        using DeviceChildImpl::GetForImmediateUse;
        const RootSignatureDesc m_Desc;
    };
};

template<> class std::hash<D3D12TranslationLayer::RootSignatureDesc>
{
public:
    size_t operator()(D3D12TranslationLayer::RootSignatureDesc const& d) const
    {
        size_t seed = std::hash<UINT64>()(d.GetAsUINT64());
        if (d.m_Flags & D3D12TranslationLayer::RootSignatureDesc::UsesShaderInterfaces)
        {
            for (auto SRVSpaces : d.m_NumSRVSpacesUsed)
            {
                D3D12TranslationLayer::hash_combine(seed, SRVSpaces);
            }
        }
        return seed;
    }
};
