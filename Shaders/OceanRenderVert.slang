struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;
}

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(world, float4(input.position, 1.0f));
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}