cbuffer Constants : register(b0)
{
    float2 textureSize;
    float2 chunkPosition;

    float amplitude;
    float frequency;
    float amplitudeScalar;
    float frequencyScalar;

    float warpStrength;
    float elevationPower;
    float ridgeBlend;
    float waterLevel;

    float valleyScalar;
    int octaves;
    uint seed;
};

// Terrain heigth map
RWTexture2D<float> OutputTexture : register(u0);

// RNG hash from https://gist.github.com/mpottinger/54d99732d4831d8137d178b4a6007d1a
uint murmurHash11(uint src)
{
    const uint M = 0x5bd1e995u;
    uint h = 1190494759u;
    src *= M;
    src ^= src >> 24u;
    src *= M;
    h *= M;
    h ^= src;
    h ^= h >> 13u;
    h *= M;
    h ^= h >> 15u;
    return h;
}

// Modified version of https://www.shadertoy.com/view/ttc3zr modified to work with hlsl
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

    float2 seedOffset = hash22(float2(seed, seed * 1.61803398));

    float2 i = floor(p + (p.x + p.y) * K1);
    float2 a = p - i + (i.x + i.y) * K2;
    float m = step(a.y, a.x);
    float2 o = float2(m, 1.0 - m);
    float2 b = a - o + K2;
    float2 c = a - 1.0 + 2.0 * K2;
    float3 h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    float3 n = h * h * h * h *
        float3(dot(a, hash22(i + seedOffset + 0.0)), dot(b, hash22(i + seedOffset + o)), dot(c, hash22(i + seedOffset + 1.0)));
    return dot(n, float3(70.0));
}

float fbm(float2 p, float2 offset)
{
    float n = 0.0f;

    float amp = amplitude;
    float freq = frequency;

    // Layer the noise at different scales FBM
    for (int i = 0; i < octaves; i++)
    {
        n += amp * evaluateNoise((p + offset) * freq);
        amp *= amplitudeScalar;
        freq *= frequencyScalar;
    }

    return n;
}

float ridgeFbm(float2 p, float2 offset, float a, float f, float o)
{
    float val = 0.0f;

    float amp = a;
    float freq = f;

    float prev = 1.0f;
    for (int i = 0; i < o; i++)
    {
        float n = evaluateNoise((p + offset) * freq);
        n = 1.0f - abs(n);
        n = n * n;
        n *= prev;
        prev = n;
        val += amp * n;
        amp *= amplitudeScalar;
        freq *= frequencyScalar;
    }

    return val;
}

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y) return;

    // -1, 1 range
    float2 center = float2(0.5f, 0.5f);
    float2 uv = float2(id.xy) / float2(textureSize);
    float dist = max(abs(uv.x - center.x), abs(uv.y - center.y));

    float2 positionInChunk = (chunkPosition + float2(id.xy)) + float2(0.1f);

    float2 falloffWarp = float2(evaluateNoise(positionInChunk * 0.003f + float2(1.7f, 9.2f)),
                                evaluateNoise(positionInChunk * 0.003f + float2(8.3f, 2.8f)));

    float2 warpedUV = float2(dist, dist) + (falloffWarp * 0.25f);
    dist = length(warpedUV);

    float falloff = smoothstep(0.5f, 1.0f, dist);
    //falloff = falloff * falloff;

    float noise = 0.0f;

    float amp = amplitude;
    float freq = frequency;

    float2 warpOffset = float2(evaluateNoise(positionInChunk * frequency * 0.4f + float2(0.0f, 0.0f)),
                               evaluateNoise(positionInChunk * frequency * 0.4f + float2(5.2f, 1.3f)));

    // 0.4 warp strength
    float2 warpedPos = positionInChunk + warpOffset * 0.4 / frequency;

    float elevation = fbm(warpedPos, float2(0.0f, 0.0f));
    float ridges = ridgeFbm(warpedPos, float2(50.0f, 100.0f), amplitude * 0.7f, frequency * 0.8f, max(octaves - 1, 2));

    // Valleys are carved in the terrain
    float valleys = fbm(warpedPos, float2(200.0f, 300.0f));
    valleys = valleys * valleys;
    valleys *= valleyScalar;

    float mask = saturate(1.0f - dist * 1.5);
    mask = mask * mask;

    float blendedElevation = lerp(elevation, ridges, ridgeBlend * mask);
    blendedElevation -= valleys * (1.0f - mask * 0.5f);

    float coastProximity = 1.0f - saturate(abs(blendedElevation) * 4.0f);
    coastProximity = coastProximity * coastProximity;
    float coastDetail = evaluateNoise(positionInChunk * frequency * 4.0f + float2(77.7f, 33.3f));
    blendedElevation += coastDetail * 0.05f * coastProximity;

    float border = min(min(uv.x, 1.0f - uv.x), min(uv.y, 1.0f - uv.y));
    float borderMask = saturate(border * 2.0f);

    // Subtract the falloff value from the noise to achieve an island like height map
    float height = blendedElevation - falloff;

    height -= (1.0f - borderMask) * 2.0f;
    //height = height * borderMask - (1.0f - borderMask);

    if (height > 0.0f) { height = pow(height, elevationPower); }

    height -= waterLevel;

    // Output into height map texture
    OutputTexture[id.xy] = height;
}
