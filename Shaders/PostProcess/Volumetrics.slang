cbuffer RootConsant : register(b0)
{
    float2 textureSize;
    float2 nearFar;
    uint srcTextureView;
    uint depthView;
    uint irradianceView;
};

struct PointLight {
    float3 position;
    float3 color;
    float intensity;
}

cbuffer SceneData : register(b1)
{
    column_major float4x4 inverseProj;
    column_major float4x4 inverseView;
    float3 cameraPosition;
    float pad;
    float3 sunDirection;
    float fogDensity;
    PointLight pointLights[50];
};

RWTexture2D outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState pointSampler : register(s0);

float4 GetWorldSpacePosition(float3 position)
{
    float4 positionVS = mul(inverseProj, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

float LinearizeDepth(float depth) { return (nearFar.x * nearFar.y) / (nearFar.y - depth * (nearFar.y - nearFar.x)); }

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(abs(1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

// TODO cash fog in 3d texture
float3 Hash3(float3 p)
{
    p = frac(p * float3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yzx + 19.19);
    return frac((p.xxy + p.yzz) * p.zyx);
}

float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f); // smoothstep

    float n000 = Hash3(i + float3(0, 0, 0)).x;
    float n100 = Hash3(i + float3(1, 0, 0)).x;
    float n010 = Hash3(i + float3(0, 1, 0)).x;
    float n110 = Hash3(i + float3(1, 1, 0)).x;
    float n001 = Hash3(i + float3(0, 0, 1)).x;
    float n101 = Hash3(i + float3(1, 0, 1)).x;
    float n011 = Hash3(i + float3(0, 1, 1)).x;
    float n111 = Hash3(i + float3(1, 1, 1)).x;

    return lerp(
        lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y), lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
}

// Two-octave FBM for a bit of shape detail without killing perf
float FogNoise(float3 worldPos)
{
    float3 p = worldPos * 0.05f + normalize(float3(1.0f, 0.0f, 0.3f)) * 1.5f;
    float n = ValueNoise(p) * 0.65 + ValueNoise(p * 2.1 + 7.3) * 0.35;
    return n;
}

float SampleDensity(float3 worldPos)
{
    float heightAbove = max(0.0, worldPos.y - 0);
    float heightMask = exp(-0.4 * heightAbove);

    float noise = FogNoise(worldPos);

    float noiseMod = saturate((noise - (1.0 - 0.85f)) / max(0.85f, 1e-5));

    return fogDensity * heightMask * noiseMod;
}

// source https://lonvanettinger.com/portfolio-pages/fog-ray-march-article-1
float CalculateAbsorption(float density, float distance)
{
    return exp(-distance * density);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y) return;
    uint2 coords = id.xy;

    const int stepCount = 32;
    const float maxDistance = 100.0f;
    const float stepSize = 0.8f;

    float2 uv = coords / textureSize.xy;

    float4 srcTexture = textures[NonUniformResourceIndex(srcTextureView)].Load(int3(coords, 0));

    float rawDepth = textures[NonUniformResourceIndex(depthView)].Load(int3(coords, 0)).r;
    float2 ndc = (float2(coords) + 0.5) / float2(textureSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 worldPosition = GetWorldSpacePosition(float3(ndc, rawDepth));

    float3 rayOrigin = cameraPosition;
    float3 rayDirection = normalize(worldPosition.xyz - rayOrigin);

    float sceneDepth = length(worldPosition.xyz - cameraPosition);
    float rayDistance = min(sceneDepth, maxDistance);
    
    // Steps along the ray
    float3 stepVector = rayDirection * stepSize;

    // Current position in the march
    float3 marchPosition = worldPosition.xyz;

    // Radiance out
    float3 radiance = float3(0.0);

    // Starting at 1.0 for forward ray marching
    float transmittance = 1.0;

    [loop]
    for (float i = 0; i < rayDistance; i += stepSize)
    {

        float3 inScatter = float3(0.6, 0.6, 0.6) * fogDensity * stepSize * transmittance;

        transmittance *= CalculateAbsorption(fogDensity, stepSize);
        radiance += inScatter * transmittance * stepSize;
        //radiance += transmittance * stepSize;

        marchPosition += stepVector;
    }

    float3 finalColor = srcTexture.rgb * transmittance + radiance;

    outputTexture.Store(coords, float4(finalColor, 1.0f));
}
