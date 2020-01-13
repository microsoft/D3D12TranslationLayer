// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    enum class RESOURCE_DIMENSION
    {
        UNKNOWN = 0,
        BUFFER = 1,
        TEXTURE1D = 2,
        TEXTURE2D = 3,
        TEXTURE2DMS = 4,
        TEXTURE3D = 5,
        TEXTURECUBE = 6,
        TEXTURE1DARRAY = 7,
        TEXTURE2DARRAY = 8,
        TEXTURE2DMSARRAY = 9,
        TEXTURECUBEARRAY = 10,
        RAW_BUFFER = 11,
        STRUCTURED_BUFFER = 12
    };

    typedef std::vector<RESOURCE_DIMENSION> TDeclVector;

    struct SShaderDecls
    {
        TDeclVector m_ResourceDecls;
        TDeclVector m_UAVDecls;
        UINT m_NumSamplers = 0;
        UINT m_NumCBs = 0;
        UINT m_OutputStreamMask = 0;
        bool m_bUsesInterfaces = false;
        UINT m_NumSRVSpacesUsed = 1;

        void Parse(UINT const* pDriverBytecode);
    };

    class Shader : public DeviceChild, public SShaderDecls
    {
    public:
#ifdef SUPPORTS_DXBC_PARSE
        Shader(ImmediateContext* pParent, std::unique_ptr<BYTE[]> byteCode, SIZE_T bytecodeSize);
        Shader(ImmediateContext* pParent, const BYTE* byteCode, SIZE_T bytecodeSize);
#endif
        Shader(ImmediateContext* pParent, std::unique_ptr<BYTE[]> DXBC, CComHeapPtr<void>& DXIL, SIZE_T dxilSize, SShaderDecls PrecomputedDecls);

        UINT OutputStreamMask() { return m_OutputStreamMask; }
        const D3D12_SHADER_BYTECODE& GetByteCode() const{ return m_Desc; }

    private:
#ifdef SUPPORTS_DXBC_PARSE
        void Init();
#endif

        std::unique_ptr<BYTE[]> const m_ByteCode;
        CComHeapPtr<void> const m_Dxil;
        D3D12_SHADER_BYTECODE const m_Desc;
    };
};