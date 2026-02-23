#include "Helper/Common.slang"
#include "Helper/PbrFormulas.slang"

cbuffer Constants : register(b0)
{
    uint emissiveView;
    uint albedoView;
    uint normalView;
    uint depthView;
};

cbuffer matrices : register(b1)
{
    column_major matrix inverseView;
    column_major matrix inverseProjection;
}

cbuffer PbrData : register(b2)
{
    float4 cameraPosition;
    float4 lightDirection;
    uint debugViewMode;
    uint numPointLights;
}

cbuffer envData : register(b3)
{
    uint irradianceMapView;
    uint specularView;
    uint brdfView;
}

struct PointLight {
    float4 colorIntensity; // rgb = color, w = intensity
    float4 lightPosition;
};

#define MAX_POINT_LIGHTS 50

cbuffer PointLightData : register(b4)
{
    PointLight pointLights[MAX_POINT_LIGHTS];
}

Texture2D textures[4096] : register(t0, space0);
TextureCube cubeTextures[4096] : register(t0, space1);

SamplerState pointSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    float4 albedo = float4(0);
    if (albedoView != 0) albedo = textures[albedoView].Sample(pointSampler, input.uv);

    float4 normal = float4(0);
    if (normalView != 0)
    {
        normal = textures[normalView].Sample(pointSampler, input.uv);
        // normal.y = -normal.y; // flip y
    }

    float4 emissive = float4(0);
    if (emissiveView != 0) emissive = textures[emissiveView].Sample(pointSampler, input.uv);

    // srgb to linear
    albedo.rgb = pow(albedo.rgb, float3(2.2f));
    emissive.rgb = pow(emissive.rgb, float3(2.2f));

    float roughness = albedo.a;
    float metallic = normal.a;
    float ao = emissive.a;
    if (ao > 1.0f) ao = 1.0f;

    // SSS and translucency
    float thickness = normal.b;

    // Reconstruct the position from the depth
    float3 positionNDC =
        float3(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, textures[depthView].Sample(pointSampler, input.uv).x);

    float3 positionWS = RestorePositionWS(positionNDC, inverseProjection, inverseView).xyz;

    float3 color = float3(0.0f, 0.0f, 0.0f);

    // shading setup
    float3 N = OctDecode(normal.xy);
    float3 V = normalize(cameraPosition.xyz - positionWS);
    float3 R = reflect(-V, N);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo.rgb, metallic);

    float ndotv = max(dot(N, V), 0.0f);

    // Directional light
    float3 L = normalize(lightDirection.xyz);
    float3 Lo = EvaluateDirectBRDF(N, V, L, albedo.rgb, F0, roughness, metallic);

    // Point lights
    for (uint i = 0; i < numPointLights; i++)
    {
        float3 pL = pointLights[i].lightPosition.xyz - positionWS;
        float distance = length(pL);
        pL = normalize(pL);

        float3 plColor = pointLights[i].colorIntensity.rgb;
        float plIntensity = pointLights[i].colorIntensity.w;
        float attenuation = plIntensity / (distance * distance);

        Lo += EvaluateDirectBRDF(N, V, pL, albedo.rgb, F0, roughness, metallic) * plColor * attenuation;
    }

    float3 irradiance = float3(0.0f);
    if (irradianceMapView > 0)
        irradiance = cubeTextures[irradianceMapView].Sample(pointSampler, N).rgb;
    else { irradiance = float3(0.2f, 0.2f, 0.2f); }

    float3 iblF = FresnelSchlickRough(ndotv, F0, roughness);
    float3 iblkS = iblF;
    float3 iblkD = 1.0 - iblkS;
    iblkD *= 1.0f - metallic;

    // Precomputed specular reflection
    const float maxLod = 4.0f;
    float3 prefilteredReflection = float3(0.0f);
    if (specularView > 0) prefilteredReflection = cubeTextures[specularView].SampleLevel(pointSampler, R, roughness * maxLod).rgb;

    float2 envBrdf = float2(1.0f, 0.0f);
    if (brdfView > 0) envBrdf = textures[brdfView].Sample(pointSampler, float2(ndotv, roughness)).rg;

    // envBrdf = max(envBrdf, 0.0f);

    float3 specularIBL =
        prefilteredReflection * iblkS; //(iblF * envBrdf.x + envBrdf.y); // Temporary disabled because the brdf is not correct

    float3 diffuse = albedo.rgb * irradiance;

    // Translucency
    float wrap = 0.5f;

    // Wrap the diffuse
    float NdotL = (dot(N, L) + wrap) / (1.0f + wrap);
    float scatterDiffuse = max(0.0f, NdotL);

    // TODO modify it so it is physically accurate
    const float distortion = 0.4f;
    float3 backScatter = normalize(L + N * distortion);

    const float p = 4.0f;
    float vDotBS = pow(saturate(dot(V, -backScatter)), p);

    float falloff = 1.3;
    float translucencyMask = (scatterDiffuse + vDotBS) * thickness;

    float3 backFaceIrradiance = float3(0.0f);
    if (irradianceMapView > 0)
        irradiance = cubeTextures[irradianceMapView].Sample(pointSampler, N).rgb;
    else { irradiance = float3(0.2f, 0.2f, 0.2f); }

    float3 scatterColor = float3(0.0f);
    if (thickness > 0) scatterColor = translucencyMask * float3(0.3f, 0.7f, 0.1f) * backFaceIrradiance;

    float3 ambient = (iblkD * diffuse + specularIBL + scatterColor) * ao;

    color = Lo + ambient + emissive.rgb;

    switch (debugViewMode)
    {
    case 0:
        return float4(color, 1.0f);
    case 1:
        return float4(albedo.rgb, 1.0f);
    case 2:
        return float4(N.xyz, 1.0f);
    case 3:
        return float4(float3(roughness), 1.0f);
    case 4:
        return float4(float3(metallic), 1.0f);
    case 5:
        return float4(scatterColor, 1.0f);
    case 6:
        return float4(float3(positionNDC.z), 1.0f);
    }

    return float4(color, 1.0f);
}
