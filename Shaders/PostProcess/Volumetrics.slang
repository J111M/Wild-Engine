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
    float scatteringDensity;
    float density;

    float4 scatteringColor;
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

struct PointLight {
    float4 colorIntensity; // rgb = color, w = intensity
    float4 lightPosition;
};

#define MAX_POINT_LIGHTS 50

cbuffer PointLightData : register(b2)
{
    PointLight pointLights[MAX_POINT_LIGHTS];
}

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
float CalculateAbsorption(float density, float distance)
{
    return exp(-distance * density);
}

float PhaseFunctionSchlick(float3 w0, float3 w1)
{
    const float k = 0.5f;

    float cosTheta = dot(w0, w1) / (length(w0) * length(w1));
    float nom = 1.0 - k * k;
    float denom = 4.0 * PI * (1.0 + k * cosTheta) * (1.0 + k * cosTheta);
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

    // Radiance out
    float3 radiance = float3(0.0);

    // Starting at 1.0 for forward ray marching
    float transmittance = 1.0;

    [loop]
    for (float i = 0; i < stepCount; i += stepSize)
    {
        // 1.0f - shadow sample
        float visibility = 1.0f;

        float3 viewDir = normalize(cameraPosition - marchPosition);

        float3 inScattering = float3(0.0f);
        for (uint plIndex = 0; plIndex < numOfPointLights; plIndex++) {
            PointLight pointLight = pointLights[plIndex];

            float3 lightToMarchPos = marchPosition - pointLight.lightPosition.xyz;
            float lightDistance = length(lightToMarchPos);

            float3 lightDirection = -normalize(lightToMarchPos);

            // In Scattering
            float phi = 50.0f; // Light intensity
            float denom = 4.0 * PI * lightDistance * lightDistance;
            float3 lightIn = CalculateAbsorption(density, lightDistance) * pointLight.colorIntensity.rgb * visibility * phi / denom;

            inScattering += lightIn * scatteringDensity * scatteringColor.rgb * PhaseFunctionSchlick(lightDirection, viewDir);
        }

        transmittance *= CalculateAbsorption(density, stepSize);
        radiance += inScattering * transmittance * stepSize;

        marchPosition += stepVector;
    }

    float3 finalColor = srcTexture.rgb;
    finalColor *= transmittance;
    finalColor += radiance;

    outputTexture.Store(coords, float4(float3(finalColor), 1.0f));
}
