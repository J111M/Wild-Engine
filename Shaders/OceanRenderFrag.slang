struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;
}

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET0 { 
    return float4(0.0f, 0.0f, 1.0f, 0.3f);
}