struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSOut {
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
}

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    uint4 texIndex;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

[shader("fragment")]
PSOut main(PSInput input)
{
    float4 albedoColor = float4(0, 0, 0, 0);
    
    if(texIndex.x != 0)
    {
        Texture2D albedo = textures[NonUniformResourceIndex(texIndex.x)];
        albedoColor = albedo.Sample(anisotropySampler, input.uv);
    }

    PSOut psOut;
    psOut.albedo = albedoColor;
    psOut.normal = float4(input.normal, 1.0f);

    return psOut;
}