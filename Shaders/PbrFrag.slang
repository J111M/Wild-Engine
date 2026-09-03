#include "Helper/Common.slang"
#include "Helper/PbrFormulas.slang"
#include "ShadowInc.slang"

import DDGI.DDGIProbeData;

static const uint INVALID_HEAP_INDEX = 0xFFFFFFFFu;

cbuffer Constants : register(b0)
{
    uint emissiveView;
    uint albedoView;
    uint normalView;
    uint depthView;
    uint4 shadowMapView;
    float biasValue;
    uint probeDataView;
};

cbuffer matrices : register(b1)
{
    column_major float4x4 inverseView;
    column_major float4x4 inverseProjection;
    column_major float4x4 viewSpace;
    float cameraFar;
}

cbuffer PbrData : register(b2)
{
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColorIntensity;
    uint debugViewMode;
    uint numPointLights;
}

cbuffer envData : register(b3)
{
    uint irradianceMapView;
    uint specularView;
    uint brdfView;
}

struct PointLight
{
    float4 colorIntensity; // rgb = color, w = intensity
    float4 lightPosition;
};

#define MAX_POINT_LIGHTS 15

cbuffer PointLightData : register(b4) { PointLight pointLights[MAX_POINT_LIGHTS]; }

cbuffer shadowMapBuffer : register(b5)
{
    float4 lightDirectionIntensity;
    ColumnMajor lightSpaceViewProj[SHADOWMAP_CASCADES];
    float4 cascadeDistance;
    float4 cascadeSplitDepthRange;
};

Texture2D textures[4096] : register(t0, space0);
TextureCube cubeTextures[4096] : register(t0, space1);

StructuredBuffer<float4> probeIrradiance : register(t0, space2);

SamplerState pointSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
SamplerState gbufferSampler : register(s2);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Shadow map implementation inspired by https://learnopengl.com/Guest-Articles/2021/CSM and modified to work with my system
float ShadowCalculation(float3 worldSpaceFragPosition, float3 normal, out int shadowLayer)
{
    float4 viewSpacePosition = mul(viewSpace, float4(worldSpaceFragPosition, 1.0f));

    float depthValue = abs(viewSpacePosition.z);

    int layer = -1;
    for (int i = 0; i < SHADOWMAP_CASCADES; ++i)
    {
        if (depthValue < cascadeDistance[i])
        {
            layer = i;
            break;
        }
    }

    if (layer == -1) { layer = SHADOWMAP_CASCADES - 1; }
    shadowLayer = layer;

    // Convert worldSpace to light space
    float4 lightSpacePosition = mul(lightSpaceViewProj[layer], float4(worldSpaceFragPosition, 1.0));

    // Perspective divide
    float3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;

    // Remap to 0, 1 range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;

    // Fragment depth
    float currentDepth = projCoords.z;

    if (currentDepth > 1.0) { return 0.0; }

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;

    // Shadow bias
    float3 biasNormal = normalize(normal);
    // Offset by shadow position
    float depthRange = cascadeSplitDepthRange[layer];
    float bias = max((biasValue / depthRange) * (1.0 - dot(normal, -normalize(lightDirectionIntensity.xyz))), 0.005 / depthRange);

    float shadow = 0.0f;
    texture2D shadowMap = textures[shadowMapView[layer]];

    uint2 textureSize = uint2(0, 0);
    shadowMap.GetDimensions(textureSize.x, textureSize.y);

    float2 texelSize = 1.0 / float2(textureSize);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(x, y) * texelSize, currentDepth - bias);
        }
    }
    shadow /= 9.0;

    return shadow;
}

// TODO get from raytracing utils need to resolve multiple include issue of static data
static const uint PROBE_IRRADIANCE_TEXELS = 6;
static const uint PROBE_DISTANCE_TEXELS = 14;
static const float PROBE_IRRADIANCE_GAMMA = 5.0f;

float3 DDGIProbePosition(DDGIProbeData probeData, uint3 probeGridCoord)
{
    return probeData.probeOrigin.xyz + probeData.probeSpacing.xyz * float3(probeGridCoord);
}

float3 DDGIAtlasUV(DDGIProbeData probeData, uint3 probeGridCoord, float3 direction, uint interiorTexels)
{
    uint probeTexels = interiorTexels + 2;
    float2 atlasSize = float2(probeData.probeCounts.x, probeData.probeCounts.z) * probeTexels;
    float2 texel = float2(probeGridCoord.xz * probeTexels + 1) + OctEncode(normalize(direction)).xy * interiorTexels;
    return float3(texel / atlasSize, float(probeGridCoord.y));
}

float3 DDGISurfaceBias(DDGIProbeData probeData, float3 N, float3 V)
{
    float minSpacing = min(probeData.probeSpacing.x, min(probeData.probeSpacing.y, probeData.probeSpacing.z));
    float3 bias = (N * probeData.normalBias + V * probeData.viewBias) * minSpacing;
    float maxLength = 0.5f * minSpacing;
    float biasLength = length(bias);
    return (biasLength > maxLength) ? bias * (maxLength / biasLength) : bias;
}

float DDGIVolumeWeight(DDGIProbeData probeData, float3 X)
{
    float3 relativeCoordinates = (X - probeData.probeOrigin.xyz) / probeData.probeSpacing.xyz;

    float volumeWeight = 1.0f;
    for (uint axis = 0; axis < 3; ++axis)
    {
        volumeWeight *= saturate(relativeCoordinates[axis]);
        volumeWeight *= saturate(float(probeData.probeCounts[axis]) - 1.0f - relativeCoordinates[axis]);
    }

    return volumeWeight;
}

float3 SampleDDGIIrradiance(DDGIProbeData probeData, float3 N, float3 V, float3 X)
{
    float volumeWeight = DDGIVolumeWeight(probeData, X);
    if (volumeWeight <= 0.0f) return float3(0.0f, 0.0f, 0.0f);

    float3 biasedPosition = X + DDGISurfaceBias(probeData, N, V);
    float3 biasedRelativeCoordinates = (biasedPosition - probeData.probeOrigin.xyz) / probeData.probeSpacing.xyz;

    int3 baseProbeCoord = clamp(int3(floor(biasedRelativeCoordinates)), int3(0, 0, 0), probeData.probeCounts.xyz - 1);
    float3 baseProbePosition = DDGIProbePosition(probeData, uint3(baseProbeCoord));
    float3 alpha = saturate((biasedPosition - baseProbePosition) / probeData.probeSpacing.xyz);

    Texture2DArray<float4> irradianceAtlas = DescriptorHandle<Texture2DArray<float4>>(probeData.irradianceView);
    Texture2DArray<float2> distanceAtlas = DescriptorHandle<Texture2DArray<float2>>(probeData.distanceView);

    float3 sumIrradiance = float3(0.0f, 0.0f, 0.0f);
    float sumWeight = 0.0f;

    for (uint i = 0; i < 8; ++i)
    {
        int3 offset = int3(i, i >> 1u, i >> 2u) & 1;
        uint3 probeGridCoord = uint3(clamp(baseProbeCoord + offset, int3(0, 0, 0), probeData.probeCounts.xyz - 1));
        float3 probePosition = DDGIProbePosition(probeData, probeGridCoord);

        float3 directionToProbe = normalize(probePosition - X);
        float3 biasedToProbe = probePosition - biasedPosition;
        float biasedDistance = length(biasedToProbe);
        float3 biasedDirectionToProbe = biasedToProbe / max(biasedDistance, 1e-6f);

        float3 trilinear = max(0.001f, lerp(1.0f - alpha, alpha, float3(offset)));
        float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        float wrapShading = (dot(directionToProbe, N) + 1.0f) * 0.5f;
        float weight = wrapShading * wrapShading + 0.2f;

        float3 distanceUV = DDGIAtlasUV(probeData, probeGridCoord, -biasedDirectionToProbe, PROBE_DISTANCE_TEXELS);
        float2 moments = 2.0f * distanceAtlas.SampleLevel(pointSampler, distanceUV, 0).xy;

        float chebyshev = 1.0f;
        if (biasedDistance > moments.x)
        {
            float variance = abs(moments.x * moments.x - moments.y);
            float distanceDelta = biasedDistance - moments.x;
            chebyshev = variance / (variance + distanceDelta * distanceDelta);
            chebyshev = max(chebyshev * chebyshev * chebyshev, 0.0f);
        }
        weight *= max(chebyshev, 0.05f);
        weight = max(weight, 0.000001f);

        const float crushThreshold = 0.2f;
        if (weight < crushThreshold) { weight *= weight * weight * (1.0f / (crushThreshold * crushThreshold)); }

        weight *= trilinearWeight;

        float3 irradianceUV = DDGIAtlasUV(probeData, probeGridCoord, N, PROBE_IRRADIANCE_TEXELS);
        float3 irradiance = irradianceAtlas.SampleLevel(pointSampler, irradianceUV, 0).rgb;
        irradiance = pow(max(irradiance, 0.0f), PROBE_IRRADIANCE_GAMMA * 0.5f);

        sumIrradiance += irradiance * weight;
        sumWeight += weight;
    }

    if (sumWeight <= 0.0f) return float3(0.0f, 0.0f, 0.0f);

    float3 result = sumIrradiance * (1.0f / sumWeight);
    result *= result;
    result *= 2.0f * PI;

    return result * volumeWeight * probeData.intensity;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    float4 albedo = float4(0);
    if (albedoView != 0) albedo = textures[albedoView].Sample(gbufferSampler, input.uv);

    float4 normal = float4(0);
    if (normalView != 0)
    {
        normal = textures[normalView].Sample(gbufferSampler, input.uv);
        // normal.y = -normal.y; // flip y
    }

    float4 emissive = float4(0);
    if (emissiveView != 0) emissive = textures[emissiveView].Sample(gbufferSampler, input.uv);

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
        float3(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, textures[depthView].Sample(gbufferSampler, input.uv).x);

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
    float3 L = -normalize(lightDirectionIntensity.xyz);
    float3 Lo = EvaluateDirectBRDF(N, V, L, albedo.rgb, F0, roughness, metallic) * lightColorIntensity.rgb *
                lightColorIntensity.a;

    float3 pointLo = float3(0, 0, 0);
    // Point lights
    for (uint i = 0; i < numPointLights; i++)
    {
        float3 pL = pointLights[i].lightPosition.xyz - positionWS;
        float distance = length(pL);
        pL = normalize(pL);

        float3 plColor = pointLights[i].colorIntensity.rgb;
        float plIntensity = pointLights[i].colorIntensity.w;
        float attenuation = plIntensity / (distance * distance);

        pointLo += EvaluateDirectBRDF(N, V, pL, albedo.rgb, F0, roughness, metallic) * plColor * attenuation;
    }

    float3 ddgiIrradiance = float3(0.0f, 0.0f, 0.0f);
    if (probeDataView != INVALID_HEAP_INDEX)
    {
        DDGIProbeData probeData = GetDDGIProbeData(probeDataView);
        ddgiIrradiance = SampleDDGIIrradiance(probeData, N, V, positionWS);
    }

    float3 irradiance = ddgiIrradiance / PI;

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
        prefilteredReflection * iblF; //(iblF * envBrdf.x + envBrdf.y); // Temporary disabled because the brdf is not correct

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

    int debugShadow = 0;
    float shadow = ShadowCalculation(positionWS, N, debugShadow);
    color = shadow * Lo + pointLo + ambient + emissive.rgb;

    // Cascade debugging
    // switch (debugShadow)
    //{
    // case 0:
    //     return float4(1, 0, 0, 1);
    // case 1:
    //     return float4(0, 1, 0, 1);
    // case 2:
    //     return float4(0, 0, 1, 1);
    // case 3:
    //     return float4(1, 1, 0, 1);
    // }

    switch (debugViewMode)
    {
    case 0:
        return float4(color, 1.0f);
    case 1:
        return float4(float3(1.0 - shadow), 1.0f);
    case 2:
        return float4(N.xyz, 1.0f);
    case 3:
        return float4(float3(roughness), 1.0f);
    case 4:
        return float4(float3(metallic), 1.0f);
    case 5:
        return float4(ddgiIrradiance, 1.0f);
    case 6:
        return float4(float3(positionNDC.z), 1.0f);
    }

    return float4(color, 1.0f);
}
