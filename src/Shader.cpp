// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

namespace D3D12TranslationLayer
{
    Shader::Shader(ImmediateContext* pParent, std::unique_ptr<BYTE[]> DXBC, CComHeapPtr<void>& DXIL, SIZE_T dxilSize, SShaderDecls PrecomputedDecls)
        : DeviceChild(pParent)
        , SShaderDecls(std::move(PrecomputedDecls))
        , m_ByteCode(std::move(DXBC))
        , m_Dxil(DXIL.Detach())
        , m_Desc({ m_Dxil, dxilSize })
    {
    }

    Shader::Shader(ImmediateContext* pParent, const void* byteCode, SIZE_T bytecodeSize, SShaderDecls PrecomputedDecls)
        : DeviceChild(pParent)
        , SShaderDecls(std::move(PrecomputedDecls))
        , m_Desc({ byteCode, bytecodeSize })
    {
    }
};
