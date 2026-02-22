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
    float pad[3];
};

cbuffer SceneData : register(b1)
{
    column_major float4x4 inverseProj;
    column_major float4x4 inverseView;
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

RWTexture2D<float4> outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState pointSampler : register(s0);

#define PI 3.14159265358979323846

float4 GetWorldSpacePosition(float3 position)
{
    float4 positionVS = mul(inverseProj, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

float LinearizeDepth(float depth) { return (nearFar.x * nearFar.y) / (nearFar.y - depth * (nearFar.y - nearFar.x)); }

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

float SampleDensity(float3 worldPos)
{
    float3 p = worldPos * 0.2 + float3(0.0, -1.0, 0.0) * 0.05;

    float noise = FBM(p, 4); // 4 octaves, increase for more detail (costs more)

    // Remap noise so thin areas go to 0
    noise = saturate(noise - 0.3); // _NoiseCutoff e.g. 0.3 - 0.5 carves out empty space

    return noise * density;
}

float SampleSunTransmittance(float3 worldPos, int steps, float stepLen)
{
    float t = 1.0;
    float3 pos = worldPos;
    for (int i = 0; i < steps; i++)
    {
        pos -= sunDirection * stepLen; // march toward sun
        float d = SampleDensity(pos);
        t *= BeersLaw(d, stepLen);
        if (t < 0.01) break;
    }
    return t;
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

    // Radiance out
    float3 radiance = float3(0.0);

    // Starting at 1.0 for forward ray marching
    float transmittance = 1.0;

    [loop]
    for (float i = 0; i < rayDistance; i += stepSize)
    {
        // 1.0f - shadow sample
        float visibility = 1.0f;

        //worldToCamera = normalize(cameraPosition - marchPosition.xyz);

        float densityS = density; //SampleDensity(marchPosition);

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
            float3 lightIn = BeersLaw(densityS, lightDistance) * pointLight.colorIntensity.rgb * visibility * phi / denom;

            inScatter += lightIn * scatteringDensity * scatteringColor.rgb *
                PhaseFunctionSchlick(normalize(lightToMarchPos), worldToCamera);
        }

        // Directional light
        //{
        //    float visibility = SampleSunTransmittance(marchPosition, 6, stepSize * 2.0);
        //    float3 sunIn = sunColorIntensity.rgb * sunColorIntensity.a * visibility;

        //    inScatter += sunIn * scatteringDensity * scatteringColor.rgb * PhaseFunctionSchlick(-sunDirection,
        //    worldToCamera);
        //}

        radiance += inScatter * transmittance * stepSize;
        transmittance *= BeersLaw(densityS, stepSize);

        marchPosition += stepVector;
    }

    float3 finalColor = srcTexture.rgb * transmittance + radiance;

    outputTexture.Store(coords, float4(float3(finalColor), 1.0f));
}
