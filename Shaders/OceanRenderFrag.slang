#include "Helper/Common.slang"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worlPosition : WORLDPOS;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float foam : FOAM;
    float waveHeight : WHEIGHT;
};

cbuffer Constants : register(b0)
{
    float4 cameraPosition;

    float4 uvScalars;

    uint displacementMapView;
    uint slopeMapView;
    uint irradianceView;
    uint specularView;

    float normalScalar;
    float reflectionScalar;
    float waveHeightScalar;
    float airBubbleDensity;

    float4 waterScatterColor;// = float3(0.0, 0.05, 0.1);
    float4 airBubblesColor;  // = float3(0.0, 0.15, 0.4);
    float4 foamColor; // 1,1,1

    float peakScatterStrength;
    float scatterStrength;
    float scatterShadowStrength;
}

cbuffer Camera : register(b1) {
    column_major float4x4 projViewMatrix;
    column_major float4x4 modelMatix;
    column_major float4x4 invModel;
}

Texture3D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

static const float PI = 3.14159265358979323846f;

// Using fresnels equation to calculate the ratio between reflection and refraction
// F0 = The surface reflection at 0
float FresnelSchlick(float cosTheta, float F0) { return F0 + (1.0 - F0) * (pow(1.0 - cosTheta, 5.0)); }

// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html Beckmann [4]
float GeometricShadowingBeckmann(float3 H, float3 V, float roughness)
{
    float HdotV = max(0.00001f, max(dot(H, V), 0.00001f));
    float c = HdotV / (roughness * sqrt(1 - HdotV * HdotV));

    float c2 = c * c;

    return c < 1.6 ? (1.0f - 1.259f * c + 0.396f * c2) / (3.535f * c + 2.181 * c2) : 0.0f;
}

// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html Beckmann [3]
float BeckmannDistribution(float NdotH, float roughness)
{
    float a2 = roughness * roughness;
    float NdotH2 = NdotH * NdotH;

    float exponent = (NdotH2 - 1) / (a2 * NdotH2);

    return exp(exponent) / (PI * a2 * (NdotH2 * NdotH2));
}

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET0
{
    float4 slope0 = textures[slopeMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.x, 0.0f), 0);
    float4 slope1 = textures[slopeMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.y, 0.33f), 0);
    float4 slope2 = textures[slopeMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.z, 0.66f), 0);
    float4 slope3 = textures[slopeMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.w, 1.0f), 0);
    float4 slope = slope0 + slope1 + slope2 + slope3;

    slope = slope * normalScalar;

    float3 N = float3(0);
    float3 mN = float3(0, 1, 0); // Macro normal
    N = normalize(float3(-slope.x, 1.0f, -slope.y));

    float3 V = normalize(cameraPosition.xyz - input.worlPosition.xyz);
    float3 L = -normalize(float3(5.0, -1.5, -5.0));
    float3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0001f);
    float NdotH = max(dot(N, H), 0.0001f);
    float NdotV = dot(N, V);

    float4 displacement1 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.x, 0.0f), 0);
    float4 displacement2 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.y, 0.33f), 0);
    float4 displacement3 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.z, 0.66f), 0);
    float4 displacement4 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.w, 1.0f), 0);
    float foam = displacement1.w;
    float a = 0.1f + foam * 1.0f; // roughness

    float viewShadowing = GeometricShadowingBeckmann(H, V, a);
    float lightShadowing = GeometricShadowingBeckmann(H, L, a);

    float G = rcp(1 + viewShadowing + lightShadowing);

    float eta = 1.33f;

    float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));

    float numerator = pow(1 - NdotV, 5 * exp(-2.69 * a));
    float denominator = 1.0f + 22.7f * pow(a, 1.5f);
    float F = saturate(R + (1 - R) * numerator / denominator);

    float3 irradiance = float3(0.0);
    if (irradianceView > 0) {
        irradiance = textureCubes[irradianceView].Sample(anisotropySampler, N).rgb;
    }

    float3 specular = irradiance * F * G * BeckmannDistribution(NdotH, a);
    specular /= 4.0f * max(0.001f, max(dot(mN, L), 0.00001f));
    specular *= NdotL;

    float3 reflect = reflect(-V, N);
    float3 specularColor = float3(0);
    if (specularView > 0) {
        specularColor = textureCubes[specularView].Sample(anisotropySampler, reflect).rgb;
        specularColor *= reflectionScalar;
    } 

    float waveHeight = max(0.0f, input.waveHeight) * waveHeightScalar;

    // k 1-4 are scalable values controlled by artists formula from
    // https://gpuopen.com/gdc-presentations/2019/gdc-2019-agtd6-interactive-water-simulation-in-atlas.pdf
    float k1 = peakScatterStrength * waveHeight * pow(max(dot(L, -V), 0.00001f), 4) *
        pow(0.5f - 0.5f * dot(L, N), 3.0f);          // The quantity of scattering based on wave height
    float k2 = scatterStrength * pow(max(NdotV, 0.001f), 2.0f); // strength of reflected light towards eye
    float k3 = scatterShadowStrength * NdotL;                         // Lambert's cosine law
    float k4 = airBubbleDensity;                 // ambient light value

    float3 scatter = (k1 + k2) * waterScatterColor.rgb * irradiance * rcp(1 + lightShadowing);
    scatter += k3 * waterScatterColor.rgb * irradiance + k4 * airBubblesColor.rgb * irradiance;

    float3 sunSpecular = specular * float3(1.0f, 0.95f, 0.9f);
    float3 iblSpecular = F * specularColor;

    float3 result = (1 - F) * scatter + sunSpecular + iblSpecular;

    result = max(0.0f, result);
    result = lerp(result, foamColor.rgb, saturate(foam));

    return float4(float3(result), 1.0f);
}
