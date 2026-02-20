struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD;
    float elevation : ELEVATION;
};

struct PSOut
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 emissive : SV_TARGET2;
}

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;

    uint heightMapIndex;
    float noiseFade;
    float waterLevel;
};

cbuffer TextureViews : register(b1)
{
    uint sandTexture;
    uint sandNormalTexture;
    uint sandRoughTexture;
    uint sandAOTexture;

    uint rockTexture;
    uint rockNormalTexture;
    uint rockRoughTexture;
    uint rockAOTexture;

    uint noiseBlendView;
}

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);
SamplerState anisotropy2Sampler : register(s1);

// Octahedral encoding
float3 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) { n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0); }
    return float3(n.xy * 0.5 + 0.5, 0.0);
}

// Modified version of https://www.shadertoy.com/view/ttc3zr to work with hlsl
uint2 murmurHash22(uint2 src)
{
    const uint M = 0x5bd1e995u;
    uint2 h = uint2(1190494759u, 2147483647u);
    src *= M;
    src ^= src >> 24u;
    src *= M;
    h *= M;
    h ^= src.x;
    h *= M;
    h ^= src.y;
    h ^= h >> 13u;
    h *= M;
    h ^= h >> 15u;
    return h;
}

// 2 outputs, 2 inputs
float2 hash22(float2 src)
{
    uint2 h = murmurHash22(asuint(src));
    return asfloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}

// The MIT License
// Copyright © 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE. https://www.youtube.com/c/InigoQuilez https://iquilezles.org Code from shadertoy:
// www.shadertoy.com/view/Msf3WH

float evaluateNoise(float2 p)
{
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;

    float2 i = floor(p + (p.x + p.y) * K1);
    float2 a = p - i + (i.x + i.y) * K2;
    float m = step(a.y, a.x);
    float2 o = float2(m, 1.0 - m);
    float2 b = a - o + K2;
    float2 c = a - 1.0 + 2.0 * K2;
    float3 h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    float3 n = h * h * h * h * float3(dot(a, hash22(i + 0.0)), dot(b, hash22(i + o)), dot(c, hash22(i + 1.0)));
    return dot(n, float3(70.0));
}

float fbm(float2 p, float2 offset)
{
    float n = 0.0f;

    float amp = 1.0f;
    float freq = 1.0f;

    // Layer the noise at different scales FBM
    for (int i = 0; i < 3; i++)
    {
        n += amp * evaluateNoise((p + offset) * freq);
        amp *= 0.5f;
        freq *= 2.0;
    }

    return n;
}

float cellNoise(float2 p)
{
    float2 cell = floor(p);
    return hash22(cell).x;
}

float4 RandomizedTextureColor(float2 uv, float3 bright, float3 dark, float3 variation, float seed)
{
    float2 warpOffset = float2(fbm(uv * 1.5, float2(1.7 + seed, 9.2 + seed)), fbm(uv * 1.5, float2(8.3 + seed, 2.8 + seed))) * 0.4;

    float largePatch = fbm(uv * 2.0 + warpOffset, float2(0.0, 0.0));
    largePatch = saturate(largePatch * 0.5 + 0.5);

    float fineDetail = fbm(uv * 12.0, float2(3.1, 5.7));
    fineDetail = saturate(fineDetail * 0.5 + 0.5);

    float clump = cellNoise(uv);

    float3 baseColor = lerp(dark, bright, largePatch);

    float highlightMask = saturate((fineDetail - 0.65) * 5.0) * saturate(largePatch - 0.4);
    baseColor = lerp(baseColor, variation, highlightMask * 0.6);

    float clumpShadow = lerp(0.75, 1.0, clump);
    baseColor *= clumpShadow;

    float baseRoughness = lerp(0.85, 0.60, largePatch);
    float roughnessDetail = fbm(uv * 18.0, float2(6.4, 1.2));
    roughnessDetail = roughnessDetail * 0.5 + 0.5;
    baseRoughness += roughnessDetail * 0.08;

    return saturate(float4(baseColor, baseRoughness));
}

float HeightFromColor(float3 color) { return dot(color, float3(0.299, 0.587, 0.114)); }

float2 MaskedBlending(float h0, float h1, float mask, float contrast) {
    // Get the depth of both textures
    float b0 = h0 + (1.0 - mask);
    float b1 = h1 + mask;

    // Sharpness of the transition
    float depth = lerp(1.0, 0.001, contrast);

    // Get tallest surface
    float rcpSum = rcp(b0 + b1);

    return float2(b0 * rcpSum, b1 * rcpSum);
}

[shader("fragment")]
PSOut main(PSInput input)
{
    float3 albedo = float3(0.0f);
    float3 geoNormal = float3(0.0f);
    float blendedRoughness = 0.0f;

    const float textureScale = 96.0f;

    float contrast = 0.6;
    // Blend weight
    float2 w;

    //  Blend values
    float t = 0.8f;
    float4 noiseBlend = textures[NonUniformResourceIndex(noiseBlendView)].Sample(anisotropy2Sampler, input.uv);
    noiseBlend = noiseBlend * 2.0f - 1.0f; // Remap -1, 1

    // Load terrain texture data
    float4 grassColor =
        RandomizedTextureColor(input.uv, float3(0.58, 0.976, 0.53), float3(0.345, 0.525, 0.07), float3(0.933, 0.784, 0.067), 612.51f);
    float grassRoughness = grassColor.w;

    grassColor.w = HeightFromColor(grassColor.rgb);

    float4 dirtColor = RandomizedTextureColor(
    input.uv, float3(0.561, 0.396, 0.215), float3(0.701, 0.519, 0.215), float3(0.886, 0.643, 0.215), 152.61f);
    float dirtRoughness = dirtColor.w;

    dirtColor.w = HeightFromColor(dirtColor.rgb);

    float4 sandColor = textures[NonUniformResourceIndex(sandTexture)].Sample(anisotropy2Sampler, input.uv * textureScale);
    float3 sandNormal =
        textures[NonUniformResourceIndex(sandNormalTexture)].Sample(anisotropy2Sampler, input.uv * textureScale).rgb * 2.0 - 1.0f;
    float sandRoughness =
        textures[NonUniformResourceIndex(sandRoughTexture)].Sample(anisotropy2Sampler, input.uv * textureScale).r;

    float4 rockColor = textures[NonUniformResourceIndex(rockTexture)].Sample(anisotropy2Sampler, input.uv * textureScale);
    float3 rockNormal =
        textures[NonUniformResourceIndex(rockNormalTexture)].Sample(anisotropy2Sampler, input.uv * textureScale).rgb * 2.0 - 1.0f;
    float rockRoughness =
        textures[NonUniformResourceIndex(rockRoughTexture)].Sample(anisotropy2Sampler, input.uv * textureScale).r;

    // Slope steepness of the terrain
    float slope = 1.0f - input.normal.y;
    float blendAmount = 0.0f;
    float maskedSlope = slope + noiseBlend.r * (noiseFade + 0.05);

    if (maskedSlope < 0.2)
    {
        blendAmount = maskedSlope / 0.2f;

        w = MaskedBlending(grassColor.a, dirtColor.a, blendAmount, contrast);

        albedo = grassColor.rgb * w.x + dirtColor.rgb * w.y;
        geoNormal = float3(0, 0, 1);
        blendedRoughness = grassRoughness * w.x + dirtRoughness * w.y;
    }

    if ((maskedSlope < 0.7) && (maskedSlope >= 0.2f))
    {
        blendAmount = (maskedSlope - 0.2f) * (1.0f / (0.7f - 0.2f));
        blendAmount = pow(blendAmount, 0.55f);

        w = MaskedBlending(dirtColor.a, rockColor.a, blendAmount, contrast);

        albedo = dirtColor.rgb * w.x + rockColor.rgb * w.y;
        geoNormal = normalize(float3(0, 0, 1) * w.x + rockNormal * w.y);
        blendedRoughness = dirtRoughness * w.x + rockRoughness * w.y;
    }

    if (maskedSlope >= 0.7)
    {
        albedo = rockColor.rgb;
        geoNormal = rockNormal;
        blendedRoughness = rockRoughness;
    }



    float elevation = input.elevation;
    float noisyHeight = (elevation / 10.0f) + (noiseBlend.r * 0.05f);

    float sandMaxHeight = -0.7f;
    float sandFadeHeight = -0.65f;

    float sandBlend = 1.0f - saturate((noisyHeight - sandMaxHeight) / (sandFadeHeight - sandMaxHeight));
    sandBlend = pow(sandBlend, 1.5f);
    if (sandBlend > 0.0f)
    {
        albedo = lerp(albedo, sandColor.rgb, sandBlend);
        geoNormal = lerp(geoNormal, float3(0, 0, 1), sandBlend);
        blendedRoughness = lerp(blendedRoughness, sandRoughness, sandBlend);
    }

    float roughness = blendedRoughness;
    float metallic = 0.0f;

    float3 T = normalize(input.tangent);
    float3 B = normalize(input.bitangent);
    float3 N = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

    float3x3 TBN = float3x3(T, B, N);

    float3 normal = normalize(mul((geoNormal.xyz), TBN));

    albedo = pow(abs(albedo), 1 / 1.65);

    PSOut psOut;
    psOut.albedo = float4(albedo, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 0.0f, metallic);
    psOut.emissive = float4(float3(0.xxx), 1.0f);

    return psOut;
}
