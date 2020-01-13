// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class RootSignature;
    enum EPipelineType
    {
        e_Draw = 0,
        e_Dispatch
    };

    struct GRAPHICS_PIPELINE_STATE_DESC
    {
        Shader* pVertexShader;
        Shader* pPixelShader;
        Shader* pGeometryShader;
        Shader* pDomainShader;
        Shader* pHullShader;
        D3D12_STREAM_OUTPUT_DESC StreamOutput;
        D3D12_BLEND_DESC BlendState;
        UINT SampleMask;
        D3D12_RASTERIZER_DESC RasterizerState;
        D3D12_DEPTH_STENCIL_DESC DepthStencilState;
        D3D12_INPUT_LAYOUT_DESC InputLayout;
        D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
        UINT NumRenderTargets;
        DXGI_FORMAT RTVFormats[8];
        DXGI_FORMAT DSVFormat;
        DXGI_SAMPLE_DESC SampleDesc;
        UINT NodeMask;


        operator D3D12_GRAPHICS_PIPELINE_STATE_DESC() const
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC Ret = {};
            Ret.VS = pVertexShader ? pVertexShader->GetByteCode() : D3D12_SHADER_BYTECODE{};
            Ret.PS = pPixelShader ? pPixelShader->GetByteCode() : D3D12_SHADER_BYTECODE{};
            Ret.GS = pGeometryShader ? pGeometryShader->GetByteCode() : D3D12_SHADER_BYTECODE{};
            Ret.DS = pDomainShader ? pDomainShader->GetByteCode() : D3D12_SHADER_BYTECODE{};
            Ret.HS = pHullShader ? pHullShader->GetByteCode() : D3D12_SHADER_BYTECODE{};
            Ret.StreamOutput = StreamOutput;
            Ret.InputLayout = InputLayout;
            Ret.BlendState = BlendState;
            Ret.DepthStencilState = DepthStencilState;
            Ret.RasterizerState = RasterizerState;

            Ret.NumRenderTargets = NumRenderTargets;
            Ret.SampleDesc = SampleDesc;
            Ret.SampleMask = SampleMask;
            memcpy(Ret.RTVFormats, RTVFormats, sizeof(RTVFormats));
            Ret.DSVFormat = DSVFormat;
            Ret.IBStripCutValue = IBStripCutValue;
            Ret.PrimitiveTopologyType = PrimitiveTopologyType;
            Ret.NodeMask = NodeMask;
            return Ret;
        }
    };


    struct COMPUTE_PIPELINE_STATE_DESC
    {
        Shader* pCompute;
        UINT NodeMask;

        operator D3D12_COMPUTE_PIPELINE_STATE_DESC() const
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC Ret = {};
            Ret.CS = pCompute->GetByteCode();
            Ret.NodeMask = NodeMask;
            return Ret;
        }
    };

    struct PipelineState : protected DeviceChildImpl<ID3D12PipelineState>
    {
    public:
        EPipelineType GetPipelineStateType() { return m_PipelineStateType; }

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC &GetGraphicsDesc()
        {
            assert(m_PipelineStateType == e_Draw);
            return Graphics.m_Desc;
        }

        const D3D12_COMPUTE_PIPELINE_STATE_DESC &GetComputeDesc()
        {
            assert(m_PipelineStateType == e_Dispatch);
            return Compute.m_Desc;
        }

        template<EShaderStage Shader>
        SShaderDecls *GetShader() { 
            switch (Shader)
            {
            case e_PS:
                return Graphics.pPixelShader;
            case e_VS:
                return Graphics.pVertexShader;
            case e_GS:
                return Graphics.pGeometryShader;
            case e_HS:
                return Graphics.pHullShader;
            case e_DS:
                return Graphics.pDomainShader;
            case e_CS:
                return Compute.pComputeShader;
            default:
                assert(false);
                return nullptr;
            }
        }
        RootSignature* GetRootSignature() { return m_pRootSignature; }

        PipelineState(ImmediateContext *pContext, const GRAPHICS_PIPELINE_STATE_DESC &desc);
        PipelineState(ImmediateContext *pContext, const COMPUTE_PIPELINE_STATE_DESC &desc);
        ~PipelineState();

        ID3D12PipelineState* GetForUse(COMMAND_LIST_TYPE CommandListType)
        {
            if (m_ThreadpoolWork)
            {
                m_ThreadpoolWork.Wait(false);
            }
            return DeviceChildImpl::GetForUse(CommandListType);
        }

    protected:
        const EPipelineType m_PipelineStateType;
        RootSignature* const m_pRootSignature;
        union
        {
            struct Graphics 
            {
                D3D12_GRAPHICS_PIPELINE_STATE_DESC m_Desc;
                Shader* pVertexShader;
                Shader* pPixelShader;
                Shader* pGeometryShader;
                Shader* pDomainShader;
                Shader* pHullShader;
            } Graphics;

            struct Compute
            {
                D3D12_COMPUTE_PIPELINE_STATE_DESC m_Desc;
                Shader* pComputeShader;
            } Compute;
        };
        std::unique_ptr<D3D12_INPUT_ELEMENT_DESC[]> spInputElements;
        std::unique_ptr<D3D12_SO_DECLARATION_ENTRY[]> spSODecls;
        UINT SOStrides[D3D12_SO_STREAM_COUNT];

        CThreadPoolWork m_ThreadpoolWork;

        template<EPipelineType Type>
        void Create();

        template<EPipelineType Type>
        void CreateImpl();
    };
};
