struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constants : register(b0)
{
    matrix model;
    uint texIndex;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 albedoColor = float4(0, 0, 0, 0);
    
    if(texIndex != 0)
    {
        Texture2D albedo = textures[NonUniformResourceIndex(texIndex)];
        albedoColor = albedo.Sample(anisotropySampler, input.uv);
    }

    return float4(albedoColor.xyz, 1.0f);
}