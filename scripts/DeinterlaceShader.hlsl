#define RootSig "RootFlags( DENY_VERTEX_SHADER_ROOT_ACCESS | " \
                           "DENY_GEOMETRY_SHADER_ROOT_ACCESS | " \
                           "DENY_DOMAIN_SHADER_ROOT_ACCESS | " \
                           "DENY_HULL_SHADER_ROOT_ACCESS), " \
                "RootConstants(num32BitConstants=1, b0), " \
                "DescriptorTable (SRV(t0) )"

// Fullscreen quad
static const float4 positions[] = {
    float4(-1, 1, 0.5, 1),  // top left
    float4(1, 1, 0.5, 1),   // top right
    float4(-1, -1, 0.5, 1), // bottom left
    float4(1, -1, 0.5, 1),  // bottom right
};

cbuffer DeinterlaceConstants : register(b0)
{
    uint topFrame;
};

struct VSOutPSIn
{
    float4 position : SV_Position;
};

// Should be called with no VB to draw 4 vertices for a fullscreen quad
[RootSignature(RootSig)]
VSOutPSIn VSMain(dword input : SV_VertexID)
{
    VSOutPSIn ret;
    ret.position = positions[input];
    return ret;
}

Texture2DArray<uint4> inputTexture : register(t0);

[RootSignature(RootSig)]
uint4 PSMain(VSOutPSIn input) : SV_Target
{
    uint4 pos = uint4(input.position.xy, 0, 0);
    pos.y -= (pos.y % 2) * topFrame;
    pos.y += (1 - (pos.y % 2)) * (1 - topFrame);
    return inputTexture.Load(pos);
}
