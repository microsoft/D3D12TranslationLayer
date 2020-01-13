// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include "pch.h"
#include <dxbcutils.h>
#include <shaderbinary.h>

namespace D3D12TranslationLayer
{
    Shader::Shader(ImmediateContext* pParent, std::unique_ptr<BYTE[]> byteCode, SIZE_T bytecodeSize)
        : DeviceChild(pParent)
        , m_ByteCode(std::move(byteCode))
        , m_Desc({ m_ByteCode.get(), bytecodeSize })
    {
        Init();
    }

    Shader::Shader(ImmediateContext* pParent, const BYTE* byteCode, SIZE_T bytecodeSize)
        : DeviceChild(pParent)
        , m_Desc({ byteCode, bytecodeSize })
    {
        Init();
    }

    void Shader::Init()
    {
        CDXBCParser DXBCParser;
        if (FAILED(DXBCParser.ReadDXBCAssumingValidSize(m_Desc.pShaderBytecode)))
        {
            return;
        }
        UINT BlobIndex = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx, 0);
        if (DXBC_BLOB_NOT_FOUND == BlobIndex)
        {
            BlobIndex = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader, 0);
        }
        if (DXBC_BLOB_NOT_FOUND == BlobIndex)
        {
            return;
        }
        const UINT* pDriverBytecode = (const UINT*)DXBCParser.GetBlob(BlobIndex);
        Parse(pDriverBytecode);
    }

    void SShaderDecls::Parse(UINT const* pDriverBytecode)
    {
        D3D10ShaderBinary::CShaderCodeParser Parser(pDriverBytecode);

        UINT declSlot = 0;
        bool bDone = false;
        while (!Parser.EndOfShader() && !bDone)
        {
            D3D10ShaderBinary::CInstruction Instruction;
            Parser.ParseInstruction(&Instruction);
            switch (Instruction.m_OpCode)
            {
            case D3D10_SB_OPCODE_DCL_RESOURCE:
            case D3D11_SB_OPCODE_DCL_RESOURCE_RAW:
            case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED:
                declSlot = Instruction.Operand(0).RegIndex();
                if (declSlot >= m_ResourceDecls.size())
                {
                    m_ResourceDecls.resize(declSlot + 1,
                        RESOURCE_DIMENSION::UNKNOWN); // throw( bad_alloc )
                }
                break;
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:
                declSlot = Instruction.Operand(0).RegIndex();
                if (declSlot >= m_UAVDecls.size())
                {
                    m_UAVDecls.resize(declSlot + 1,
                        RESOURCE_DIMENSION::UNKNOWN); // throw( bad_alloc )
                }
                break;
            case D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER:
                m_NumCBs = max(m_NumCBs, Instruction.Operand(0).RegIndex() + 1);
                break;
            case D3D10_SB_OPCODE_DCL_SAMPLER:
                m_NumSamplers = max(m_NumSamplers, Instruction.Operand(0).RegIndex() + 1);
                break;

            case D3D11_SB_OPCODE_DCL_STREAM:
            {
                UINT StreamIndex = Instruction.Operand(0).RegIndex();

                m_OutputStreamMask |= (1 << StreamIndex);
            }
            break;

            }

            switch (Instruction.m_OpCode)
            {
            case D3D10_SB_OPCODE_DCL_RESOURCE:                          m_ResourceDecls[declSlot] = (RESOURCE_DIMENSION)Instruction.m_ResourceDecl.Dimension; break;
            case D3D11_SB_OPCODE_DCL_RESOURCE_RAW:                      m_ResourceDecls[declSlot] = RESOURCE_DIMENSION::BUFFER; break;
            case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED:               m_ResourceDecls[declSlot] = RESOURCE_DIMENSION::BUFFER; break;
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:       m_UAVDecls[declSlot] = (RESOURCE_DIMENSION)Instruction.m_TypedUAVDecl.Dimension; break;
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:         m_UAVDecls[declSlot] = RESOURCE_DIMENSION::BUFFER; break;
            case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED:  m_UAVDecls[declSlot] = RESOURCE_DIMENSION::BUFFER; break;

            case D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER:
            case D3D10_SB_OPCODE_DCL_SAMPLER:
            case D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE:
            case D3D10_SB_OPCODE_DCL_TEMPS:
            case D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP:
            case D3D10_SB_OPCODE_DCL_INPUT:
            case D3D10_SB_OPCODE_DCL_INPUT_SIV:
            case D3D10_SB_OPCODE_DCL_INPUT_SGV:
            case D3D10_SB_OPCODE_DCL_INPUT_PS:
            case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV:
            case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV:
            case D3D10_SB_OPCODE_DCL_OUTPUT:
            case D3D10_SB_OPCODE_DCL_OUTPUT_SGV:
            case D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
            case D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
            case D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT:
            case D3D10_SB_OPCODE_DCL_INDEX_RANGE:
            case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS:
            case D3D11_SB_OPCODE_HS_DECLS:
            case D3D11_SB_OPCODE_HS_CONTROL_POINT_PHASE:
            case D3D11_SB_OPCODE_HS_FORK_PHASE:
            case D3D11_SB_OPCODE_HS_JOIN_PHASE:
            case D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
            case D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
            case D3D11_SB_OPCODE_DCL_TESS_DOMAIN:
            case D3D11_SB_OPCODE_DCL_TESS_PARTITIONING:
            case D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:
            case D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR:
            case D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
            case D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
            case D3D11_SB_OPCODE_DCL_FUNCTION_BODY:
            case D3D11_SB_OPCODE_DCL_FUNCTION_TABLE:
            case D3D11_SB_OPCODE_DCL_INTERFACE:
            case D3D10_SB_OPCODE_CUSTOMDATA:
            case D3D11_SB_OPCODE_DCL_THREAD_GROUP:
            case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
            case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED:
            case D3D10_SB_OPCODE_DCL_OUTPUT_SIV:
            case D3D11_SB_OPCODE_DCL_STREAM:
                break;

            default:
                // Stop compilation at the first non-declaration instruction
                bDone = true;
                break;
            }
        }

        // From the DX11.1 spec: 
        // "If no streams are declared, output and 
        // output topology declarations are assumed to be 
        // for stream 0."
        if (0 == m_OutputStreamMask)
        {
            m_OutputStreamMask = 1;
        }
    }
};
