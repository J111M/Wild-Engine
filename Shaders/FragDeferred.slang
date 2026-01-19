struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSOut
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 emissive : SV_TARGET2;
}

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 invTransposeModel;
    uint albedoView;
    uint normalView;
    uint roughnessMetallicView;
    uint emissiveView;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

[shader("fragment")]
PSOut main(PSInput input)
{
    float4 albedo = float4(0, 0, 0, 0);
    if (albedoView != 0)
    {
        albedo = textures[NonUniformResourceIndex(albedoView)].Sample(anisotropySampler, input.uv);
    }

    float4 normal = float4(0, 0, 0, 0);
    if (normalView != 0)
    {
        normal.xyz = mul((float3x3)invTransposeModel, input.normal.xyz);
        // normal = textures[NonUniformResourceIndex(normalView)].Sample(anisotropySampler, input.uv);
    }
    else
    {
        normal.xyz = mul((float3x3)invTransposeModel, input.normal.xyz);
    }

    normal.xyz = normalize(normal.xyz);

    float4 emissive = float4(0, 0, 0, 0);
    if (emissiveView != 0)
    {
        emissive = textures[NonUniformResourceIndex(emissiveView)].Sample(anisotropySampler, input.uv);
    }

    float roughness = 0.0f;
    float metallic = 0.0f;
    if (roughnessMetallicView != 0)
    {
        float2 rm = textures[NonUniformResourceIndex(roughnessMetallicView)].Sample(anisotropySampler, input.uv).gb;
        roughness = rm.r;
        metallic = rm.g;
    }

    PSOut psOut;
    psOut.albedo = float4(albedo.rgb, roughness);
    psOut.normal = float4(normal.rgb, metallic);
    psOut.emissive = float4(emissive.rgb, 0.0f);

    return psOut;
}
