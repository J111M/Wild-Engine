#include "Helper/Common.slang"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    nointerpolation uint meshletId : MESHLETID;
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
SamplerState anisotropicWrapSampler : register(s0);

float3 MeshletDebugColor(uint meshletId)
{
    uint hash = meshletId;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;

    float hue = float(hash & 0xFFFFFFu) / float(0x1000000u);
    float3 rgb = saturate(abs(frac(hue + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);

    const float saturation = 0.85f;
    return lerp(float3(1.0f), rgb, saturation);
}

[shader("fragment")]
PSOut main(PSInput input)
{
    // Cluster debug view, the colour goes in emissive with a black albedo so the lighting
    // pass hands it straight through instead of shading it
    if (input.meshletId != MESHLET_DEBUG_DISABLED)
    {
        float3 debugNormal = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

        // A black albedo with a fully metallic surface leaves the lighting pass nothing to
        // work with, so the cluster colour comes out of the emissive target untouched
        PSOut debugOut;
        debugOut.albedo = float4(0, 0, 0, 1);
        debugOut.normal = float4(float2(OctEncode(debugNormal).xy), 0.0f, 1.0f);
        debugOut.emissive = float4(MeshletDebugColor(input.meshletId), 1.0f);
        return debugOut;
    }

    float4 albedo = float4(0, 0, 0, 0);
    if (albedoView != 0) { albedo = textures[NonUniformResourceIndex(albedoView)].Sample(anisotropicWrapSampler, input.uv); }
    else {
        albedo = float4(1, 1, 1, 1);
    }

    float4 normal = float4(0, 0, 0, 0);
    if (normalView != 0)
    {
        normal.xyz = textures[NonUniformResourceIndex(normalView)].Sample(anisotropicWrapSampler, input.uv).xyz;
        normal.xyz = normal.xyz * 2.0 - 1.0;
        //normal.xyz = mul((float3x3)invTransposeModel, normal.xyz);

        float3 geoNormal = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

        float4 tangent = input.tangent;

        float3 T = normalize(mul((float3x3)model, tangent.xyz));
        T = normalize(T - dot(T, geoNormal) * geoNormal);
        float3 B = cross(geoNormal, T) * tangent.w;

        float3x3 TBN = float3x3(T, B, geoNormal);

        normal.xyz = normalize(mul((normal.xyz), TBN));
    }
    else {
        normal.xyz = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));
    }

    float4 emissive = float4(0, 0, 0, 0);
    if (emissiveView != 0)
    {
        emissive = textures[NonUniformResourceIndex(emissiveView)].Sample(anisotropicWrapSampler, input.uv);
    }

    float roughness = 0.8f;
    float metallic = 0.0f;
    
    if (roughnessMetallicView != 0)
    {
        float2 rm = textures[NonUniformResourceIndex(roughnessMetallicView)].Sample(anisotropicWrapSampler, input.uv).gb;
        roughness = rm.x;
        metallic = rm.y;
    }

    PSOut psOut;
    psOut.albedo = float4(albedo.rgb, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 0.0f, metallic);
    psOut.emissive = float4(emissive.rgb, 1.0f);

    return psOut;
}
