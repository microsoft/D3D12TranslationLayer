// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    PipelineState::PipelineState(ImmediateContext *pContext, const GRAPHICS_PIPELINE_STATE_DESC &desc)
        : DeviceChildImpl(pContext)
        , m_PipelineStateType(e_Draw)
        , m_pRootSignature(pContext->CreateOrRetrieveRootSignature(
            RootSignatureDesc(desc.pVertexShader,
                              desc.pPixelShader,
                              desc.pGeometryShader,
                              desc.pHullShader,
                              desc.pDomainShader,
                              pContext->RequiresBufferOutofBoundsHandling())))
    {
        Graphics.m_Desc = desc;
        Graphics.m_Desc.pRootSignature = m_pRootSignature->GetForImmediateUse();
        Graphics.pVertexShader = desc.pVertexShader;
        Graphics.pGeometryShader = desc.pGeometryShader;
        Graphics.pPixelShader = desc.pPixelShader;
        Graphics.pDomainShader = desc.pDomainShader;
        Graphics.pHullShader = desc.pHullShader;

        // Perform deep copy of embedded arrays
        // Note: decls containing strings reference their strings from shader bytecode and do not need to deep copy them here
        if (desc.InputLayout.NumElements)
        {
            spInputElements.reset(new D3D12_INPUT_ELEMENT_DESC[desc.InputLayout.NumElements]); // throw( bad_alloc )
            memcpy(spInputElements.get(), desc.InputLayout.pInputElementDescs, sizeof(spInputElements[0]) * desc.InputLayout.NumElements);
            Graphics.m_Desc.InputLayout.pInputElementDescs = spInputElements.get();
        }
        if (desc.StreamOutput.NumEntries)
        {
            spSODecls.reset(new D3D12_SO_DECLARATION_ENTRY[desc.StreamOutput.NumEntries]); // throw( bad_alloc )
            memcpy(spSODecls.get(), desc.StreamOutput.pSODeclaration, sizeof(spSODecls[0]) * desc.StreamOutput.NumEntries);
            Graphics.m_Desc.StreamOutput.pSODeclaration = spSODecls.get();
        }
        if (desc.StreamOutput.pBufferStrides)
        {
            memcpy(SOStrides, desc.StreamOutput.pBufferStrides, sizeof(UINT) * desc.StreamOutput.NumStrides);
            Graphics.m_Desc.StreamOutput.pBufferStrides = SOStrides;
        }

        Create<e_Draw>();
    }

    PipelineState::PipelineState(ImmediateContext *pContext, const COMPUTE_PIPELINE_STATE_DESC &desc)
        : DeviceChildImpl(pContext)
        , m_PipelineStateType(e_Dispatch)
        , m_pRootSignature(pContext->CreateOrRetrieveRootSignature(
            RootSignatureDesc(desc.pCompute,
                              pContext->RequiresBufferOutofBoundsHandling())))
    {
        Compute.m_Desc = desc;
        Compute.m_Desc.pRootSignature = m_pRootSignature->GetForImmediateUse();
        Compute.pComputeShader = desc.pCompute;

        Create<e_Dispatch>();
    }

    PipelineState::~PipelineState()
    {
        if (m_pParent->GetPipelineState() == this)
        {
            m_pParent->SetPipelineState(nullptr);
        }
    }

    template<EPipelineType Type> struct PSOTraits;
    template<> struct PSOTraits<e_Draw>
    {
        static decltype(&ID3D12Device::CreateGraphicsPipelineState) GetCreate() { return &ID3D12Device::CreateGraphicsPipelineState; }
        static const D3D12_GRAPHICS_PIPELINE_STATE_DESC &GetDesc(PipelineState &p) { return p.GetGraphicsDesc(); }
    };

    template<> struct PSOTraits<e_Dispatch>
    {
        static decltype(&ID3D12Device::CreateComputePipelineState) GetCreate() { return &ID3D12Device::CreateComputePipelineState; }
        static const D3D12_COMPUTE_PIPELINE_STATE_DESC &GetDesc(PipelineState &p) { return p.GetComputeDesc(); }
    };


    template<EPipelineType Type>
    inline void PipelineState::Create()
    {
        if (m_pParent->m_spPSOCompilationThreadPool)
        {
            m_pParent->m_spPSOCompilationThreadPool->QueueThreadpoolWork(m_ThreadpoolWork,
            [this]()
            {
                try
                {
                    CreateImpl<Type>();
                }
                catch (_com_error&) {}
            });
        }
        else
        {
            CreateImpl<Type>();
        }
    }

    template<EPipelineType Type>
    inline void PipelineState::CreateImpl()
    {
        typedef PSOTraits<Type> PSOTraits;

        HRESULT hr = (m_pParent->m_pDevice12.get()->*PSOTraits::GetCreate())(&PSOTraits::GetDesc(*this), IID_PPV_ARGS(GetForCreate()));
        if (FAILED(hr))
        {
            MICROSOFT_TELEMETRY_ASSERT(hr != E_INVALIDARG);
            if (g_hTracelogging)
            {
                TraceLoggingWrite(g_hTracelogging,
                                  "PSOCreationFailure",
                                  TraceLoggingInt32(0, "SchemaVersion"),
                                  TraceLoggingHResult(hr, "HResult"),
                                  TraceLoggingInt32(Type, "PSOType"),
                                  TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                                  TraceLoggingLevel(TRACE_LEVEL_ERROR));
            }
        }
        ThrowFailure(hr); // throw( _com_error )

        if (m_pParent->UseRoundTripPSOs())
        {
            CComPtr<ID3DBlob> spBlob;
            ThrowFailure(GetForImmediateUse()->GetCachedBlob(&spBlob)); // throw( _com_error )

            if constexpr (Type == e_Draw)
            {
                Graphics.m_Desc.CachedPSO.pCachedBlob = spBlob->GetBufferPointer();
                Graphics.m_Desc.CachedPSO.CachedBlobSizeInBytes = spBlob->GetBufferSize();
            }
            else
            {
                Compute.m_Desc.CachedPSO.pCachedBlob = spBlob->GetBufferPointer();
                Compute.m_Desc.CachedPSO.CachedBlobSizeInBytes = spBlob->GetBufferSize();
            }

            hr = (m_pParent->m_pDevice12.get()->*PSOTraits::GetCreate())(&PSOTraits::GetDesc(*this), IID_PPV_ARGS(GetForCreate()));
            ThrowFailure(hr); // throw( _com_error )
        }
    }
}