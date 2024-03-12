#define RootSig "RootFlags( DENY_GEOMETRY_SHADER_ROOT_ACCESS | " \
                           "DENY_DOMAIN_SHADER_ROOT_ACCESS | " \
                           "DENY_HULL_SHADER_ROOT_ACCESS), " \
                "DescriptorTable (SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL),"  /* [0] SRVs for the possible 3 planes*/ \
                "RootConstants(num32BitConstants=6, b1, visibility=SHADER_VISIBILITY_VERTEX),"      /* [1] src info */ \
                "RootConstants(num32BitConstants=5, b2, visibility=SHADER_VISIBILITY_PIXEL),"       /* [2] pixel format info */ \
                "StaticSampler(s0, filter=FILTER_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP, visibility=SHADER_VISIBILITY_PIXEL)"   /* [2] sampler */ \

// texel coordinates
cbuffer srcInfo: register(b1)
{
    int g_srcLeft;
    int g_srcRight;
    int g_srcTop;
    int g_srcBottom;
    int g_srcWidth;
    int g_srcHeight;
};

cbuffer pixelInfo: register(b2)
{
    float4 g_colorKey;
    int g_srcPixelScalingFactor;
};

struct VSOutPSIn
{
    float4 position : SV_Position;
    float2 texcoordsNorm : TEXCOORD0;
    int2   texcoords : TEXCOORD1;
};

SamplerState g_linearSampler : register(s0);    // Linear sampler

// Uses given position from constant buffer
[RootSignature(RootSig)]
VSOutPSIn VSMain(dword input : SV_VertexID)
{
    VSOutPSIn ret;
    switch (input)
    {
    case 0:
        ret.position = float4(-1.0f, 1.0f, 0.5f, 1.0f);
        ret.texcoords = int2(g_srcLeft, g_srcTop);
        break;
    case 1:
        ret.position = float4(1.0f, 1.0f, 0.5f, 1.0f);
        ret.texcoords = int2(g_srcRight, g_srcTop);
        break;
    case 2:
        ret.position = float4(-1.0f, -1.0f, 0.5f, 1.0f);
        ret.texcoords = int2(g_srcLeft, g_srcBottom);
        break;
    case 3:
        ret.position = float4(1.0f, -1.0f, 0.5f, 1.0f);
        ret.texcoords = int2(g_srcRight, g_srcBottom);
        break;
    }
    ret.texcoordsNorm = ret.texcoords / float2(g_srcWidth, g_srcHeight);
    return ret;
}

// The texture in an arbitrary format taken from the target process
Texture2D<float4> inputTexture : register(t0);

//Textures for planar YUV pixelShader variants
Texture2D <float2> inputTexturePlane1 : register(t1);
Texture2D <float2> inputTexturePlane2 : register(t2);

float4 YUVToRGB(float4 YUVA)
{
    float C = (YUVA.r * 256) - 16;
    float D = (YUVA.g * 256) - 128;
    float E = (YUVA.b * 256) - 128;

    float R = clamp(( 298 * C           + 409 * E + 128) / 256, 0, 256);
    float G = clamp(( 298 * C - 100 * D - 208 * E + 128) / 256, 0, 256);
    float B = clamp(( 298 * C + 516 * D           + 128) / 256, 0, 256);

    return float4(R / 256, G / 256, B / 256, YUVA.a);
}

//////////////////////////////////////////////////
//                                              //
//                COLORKEY_NONE                 //
//                                              //
//////////////////////////////////////////////////
// Basic visualization, works for image data
[RootSignature(RootSig)]
float4 PSBasic(VSOutPSIn input) : SV_Target
{
    return inputTexture.Sample(g_linearSampler, input.texcoordsNorm);
}
[RootSignature(RootSig)]
float4 PSBasic_SwapRB(VSOutPSIn input) : SV_Target
{
    return inputTexture.Sample(g_linearSampler, input.texcoordsNorm).zyxw;
}

[RootSignature(RootSig)]
float4 PSAlphaOnly(VSOutPSIn input) : SV_Target
{
    float4 color = inputTexture.Sample(g_linearSampler, input.texcoordsNorm);
    color.rgb = color.a;
    color.a = 1.0;
    return color;
}

[RootSignature(RootSig)]
float4 PSDepth(VSOutPSIn input) : SV_Target
{
    float4 rawData = float4(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).rrr, 1);
    return rawData * rawData;
}

// DXGI_FORMAT_AYUV -> float4(V, U, Y, A) (no subsampling)
[RootSignature(RootSig)]
float4 PSAYUV(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = inputTexture.Sample(g_linearSampler, input.texcoordsNorm).bgra;
    return YUVToRGB(YUVA);
}

// DXGI_FORMAT_Y410/Y416 -> float4(U, Y, V, A) (no subsampling)
[RootSignature(RootSig)]
float4 PSY4XX(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = inputTexture.Sample(g_linearSampler, input.texcoordsNorm).grba;
    return YUVToRGB(YUVA);
}

// DXGI_FORMAT_YUY2/Y210/Y216 -> float4(Y0, U, Y1, V) (4:2:2 subsampled)
[RootSignature(RootSig)]
float4 PSPackedYUV(VSOutPSIn input) : SV_TARGET
{
    float4 YUYV = inputTexture.Sample(g_linearSampler, float2(input.texcoordsNorm.x, input.texcoordsNorm.y));
    float4 YUVA = float4(input.texcoords.x % 2 == 0 ? YUYV.r : YUYV.b, YUYV.ga, 1);
    return YUVToRGB(YUVA);
}

// DXGI_FORMAT_NV12/NV11/P010/P016/P208/420_OPAQUE -> t0.r = Y, t1.rg = UV (4:2:0, 4:2:2, or 4:1:1 subsampled)
[RootSignature(RootSig)]
float4 PS2PlaneYUV(VSOutPSIn input) : SV_TARGET
{
    float3 inputYUV = float3(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane1.Sample(g_linearSampler, input.texcoordsNorm).rg);
    float4 scaledYUVA = float4(inputYUV.r * g_srcPixelScalingFactor, inputYUV.g * g_srcPixelScalingFactor, inputYUV.b * g_srcPixelScalingFactor, 1);
    return YUVToRGB(scaledYUVA);
}

// DXGI_FORMAT_V208/V408 -> t0.r = Y, t1.r = U, t2.r = V (4:4:0 or 4:4:4 subsampled)
[RootSignature(RootSig)]
float4 PS3PlaneYUV(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = float4(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane1.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane2.Sample(g_linearSampler, input.texcoordsNorm).r, 1);
    return YUVToRGB(YUVA);
}

//////////////////////////////////////////////////
//                                              //
//                COLORKEY_SRC                  //
//                                              //
//////////////////////////////////////////////////

#define returnOrDiscard(col)                \
    float4 color = col;                     \
    if(all(color.rgb == g_colorKey.rgb))    \
    {                                       \
        discard;                            \
    }                                       \
    return color;

[RootSignature(RootSig)]
float4 PSBasic_ColorKeySrc(VSOutPSIn input) : SV_Target
{
    returnOrDiscard(inputTexture.Sample(g_linearSampler, input.texcoordsNorm));
}
[RootSignature(RootSig)]
float4 PSBasic_SwapRB_ColorKeySrc(VSOutPSIn input) : SV_Target
{
    returnOrDiscard(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).zyxw);
}

// DXGI_FORMAT_AYUV -> float4(V, U, Y, A) (no subsampling)
[RootSignature(RootSig)]
float4 PSAYUV_ColorKeySrc(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = inputTexture.Sample(g_linearSampler, input.texcoordsNorm).bgra;
    returnOrDiscard(YUVToRGB(YUVA));
}

// DXGI_FORMAT_Y410/Y416 -> float4(U, Y, V, A) (no subsampling)
[RootSignature(RootSig)]
float4 PSY4XX_ColorKeySrc(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = inputTexture.Sample(g_linearSampler, input.texcoordsNorm).grba;
    returnOrDiscard(YUVToRGB(YUVA));
}

// DXGI_FORMAT_YUY2/Y210/Y216 -> float4(Y0, U, Y1, V) (4:2:2 subsampled)
[RootSignature(RootSig)]
float4 PSPackedYUV_ColorKeySrc(VSOutPSIn input) : SV_TARGET
{
    float4 YUYV = inputTexture.Sample(g_linearSampler, float2(input.texcoordsNorm.x, input.texcoordsNorm.y));
    float4 YUVA = float4(input.texcoords.x % 2 == 0 ? YUYV.r : YUYV.b, YUYV.ga, 1);
    returnOrDiscard(YUVToRGB(YUVA));
}

// DXGI_FORMAT_NV12/NV11/P010/P016/P208/420_OPAQUE -> t0.r = Y, t1.rg = UV (4:2:0, 4:2:2, or 4:1:1 subsampled)
[RootSignature(RootSig)]
float4 PS2PlaneYUV_ColorKeySrc(VSOutPSIn input) : SV_TARGET
{
    float3 inputYUV = float3(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane1.Sample(g_linearSampler, input.texcoordsNorm).rg);
    float4 scaledYUVA = float4(inputYUV.r * g_srcPixelScalingFactor, inputYUV.g * g_srcPixelScalingFactor, inputYUV.b * g_srcPixelScalingFactor, 1);
    returnOrDiscard(YUVToRGB(scaledYUVA));
}

// DXGI_FORMAT_V208/V408 -> t0.r = Y, t1.r = U, t2.r = V (4:4:0 or 4:4:4 subsampled)
[RootSignature(RootSig)]
float4 PS3PlaneYUV_ColorKeySrc(VSOutPSIn input) : SV_TARGET
{
    float4 YUVA = float4(inputTexture.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane1.Sample(g_linearSampler, input.texcoordsNorm).r, inputTexturePlane2.Sample(g_linearSampler, input.texcoordsNorm).r, 1);
    returnOrDiscard(YUVToRGB(YUVA));
}

#undef returnOrDiscard