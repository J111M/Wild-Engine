#include "../ShadowInc.slang"
#include "Formula.slang"

cbuffer RootConsant : register(b0)
{
    float2 textureSize;
    float2 nearFar;

    uint srcTextureView;
    uint depthView;
    uint irradianceView;
    uint numOfPointLights;

    uint stepCount;
    float stepSize;
    float scatteringDensity; // sigma_s
    float density;           // sigma_t

    float4 scatteringColor;

    float4 sunColorIntensity;

    float anisotropy;
    float biasValue;
    float2 pad;

    uint4 shadowMapView;
};

cbuffer SceneData : register(b1)
{
    column_major float4x4 inverseProj;
    column_major float4x4 inverseView;
    column_major float4x4 viewSpace;
    float3 cameraPosition;
    float pad;
    float3 sunDirection;
    float pad1;
};

struct PointLight
{
    float4 colorIntensity; // rgb = color, w = intensity
    float4 lightPosition;
};

#define MAX_POINT_LIGHTS 50

cbuffer PointLightData : register(b2) { PointLight pointLights[MAX_POINT_LIGHTS]; }

cbuffer shadowMapBuffer : register(b3)
{
    float4 lightDirectionIntensity;
    ColumnMajor lightSpaceViewProj[SHADOWMAP_CASCADES]; // When using an array it forces it to row major automatically ! might not
                                                        // be thesame for all drivers so has to be tested
    float4 cascadeDistance;                             // Distances are store per float value
    float4 cascadeSplitDepthRange;
};

RWTexture2D<float4> outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);

SamplerState pointSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

#define PI 3.14159265358979323846

///  Pseudo-random function https://thebookofshaders.com/10/
float randomOffset(float2 pos)
{
    return fract(sin(dot(pos.xy,
                         float2(145.0, 7.233))) *
        4274.84205094213454340);
}

float4 GetWorldSpacePosition(float3 position)
{
    float4 positionVS = mul(inverseProj, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

// Shadow sample
float ShadowCalculation(float3 marchPosition, float3 normal, out int layerDepth)
{
    float4 viewSpacePosition = mul(viewSpace, float4(marchPosition, 1.0f));

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
    layerDepth = layer;

    // Convert worldSpace to light space
    float4 lightSpacePosition = mul(lightSpaceViewProj[layer], float4(marchPosition, 1.0));

    // Perspective divide
    float3 projCoords = lightSpacePosition.xyz / lightSpacePosition.w;

    // Remap to 0, 1 range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.y = 1.0 - projCoords.y;

    // Fragment depth
    float currentDepth = projCoords.z;

    if (currentDepth > 1.0) { return 1.0; }

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 1.0;

    // Shadow bias
    //float3 biasNormal = normalize(normal);
    // Offset by shadow position
    //float depthRange = cascadeSplitDepthRange[layer];
    //float bias = max((biasValue / depthRange) * (1.0 - dot(normal, normalize(-lightDirectionIntensity.xyz))), 0.005 / depthRange);

    float bias = biasValue / cascadeSplitDepthRange[layer];
    float shadow = textures[shadowMapView[layer]].SampleCmpLevelZero(
    shadowSampler, projCoords.xy, currentDepth - bias);

    return shadow;
}

float LinearizeDepth(float depth) { return (nearFar.x * nearFar.y) / (nearFar.y - depth * (nearFar.y - nearFar.x)); }

float SampleHeightBasedDensity(float height)
{
    float heightDensity = exp(-max(0.0, height - 5.0) * 0.08);
    return heightDensity * density;
}

// source https://lonvanettinger.com/portfolio-pages/fog-ray-march-article-1
float BeersLaw(float density, float distance) { return exp(-distance * density); }

float PhaseFunctionSchlick(float3 w0, float3 w1)
{
    const float g = anisotropy;

    float cosTheta = dot(w0, w1) / (length(w0) * length(w1));
    float nom = 1.0 - g * g;
    float denom = 4.0 * PI * (1.0 + g * cosTheta) * (1.0 + g * cosTheta);
    return nom / denom;
}


[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y) return;
    uint2 coords = id.xy;

    float2 uv = coords / textureSize.xy;

    const float maxDistance = 100.0f;

    float4 srcTexture = textures[NonUniformResourceIndex(srcTextureView)].Load(int3(coords, 0));

    float rawDepth = textures[NonUniformResourceIndex(depthView)].Load(int3(coords, 0)).r;
    float2 ndc = (float2(coords) + 0.5) / float2(textureSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 worldPosition = GetWorldSpacePosition(float3(ndc, rawDepth));

    float3 rayOrigin = cameraPosition;
    float3 rayDirection = normalize(worldPosition.xyz - rayOrigin);

    float sceneDepth = length(worldPosition.xyz - cameraPosition);
    float rayDistance = min(sceneDepth, maxDistance);

    float3 worldToCamera = normalize(cameraPosition - worldPosition.xyz);

    // Steps along the ray
    float3 stepVector = rayDirection * stepSize;

    // Current position in the march
    float3 marchPosition = rayOrigin;
    float jitter = randomOffset(worldPosition.xy + worldPosition.z);
    marchPosition +=  (stepVector * jitter);

    // Radiance out
    float3 radiance = float3(0.0);

    // Starting at 1.0 for forward ray marching
    float transmittance = 1.0;

    [loop]
    for (float i = 0; i < rayDistance; i += stepSize)
    {
        float densitySample = SampleHeightBasedDensity(marchPosition.y);

        float3 inScatter = float3(0.0f);
        for (uint plIndex = 0; plIndex < numOfPointLights; plIndex++)
        {
            PointLight pointLight = pointLights[plIndex];

            float3 lightToMarchPos = marchPosition - pointLight.lightPosition.xyz;
            float lightDistance = length(lightToMarchPos);

            float3 lightDirection = -normalize(lightToMarchPos);

            // In Scattering
            float phi = pointLight.colorIntensity.a; // Light intensity
            float denom = 4.0 * PI * lightDistance * lightDistance;
            float3 lightIn = BeersLaw(densitySample, lightDistance) * pointLight.colorIntensity.rgb * 1.0f * phi / denom; // TODO apply point light shadows

            inScatter += lightIn * scatteringDensity * scatteringColor.rgb *
                PhaseFunctionSchlick(normalize(lightToMarchPos), worldToCamera);
        }

        // Directional light
        {
            float3 sunDirection = normalize(-lightDirectionIntensity.xyz);
            float sunIntensity = lightDirectionIntensity.w;

            // 1.0f - shadow sample
            int layer;
            float visibility = ShadowCalculation(marchPosition, float3(0, 0, 0), layer);

            float3 lightIn = BeersLaw(densitySample, stepSize) * sunIntensity * visibility;

            switch (layer)
            {
            case 0: inScatter += float3(1 / 5, 0, 0);
            case 1: inScatter += float3(0, 1 / 5, 0);
            case 2: inScatter += float3(0, 0, 1 / 5);
            case 3: inScatter += float3(1 / 5, 1 / 5, 0);
            }

            float3 sunInScatter =
                lightIn * scatteringDensity * scatteringColor.rgb * PhaseFunctionSchlick(-sunDirection, worldToCamera);

            inScatter += sunInScatter;
        }

        radiance += inScatter * transmittance * stepSize;
        transmittance *= BeersLaw(densitySample, stepSize);

        marchPosition += stepVector;
    }

    float3 finalColor = srcTexture.rgb * transmittance + radiance;

    

    outputTexture.Store(coords, float4(float3(finalColor), 1.0f));
}
