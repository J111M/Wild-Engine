cbuffer Constants : register(b0)
{
    float2 textureSize;
    float2 chunkPosition;
    float amplitude;
    float frequency;
    float amplitudeScalar;
    float frequencyScalar;
    int octaves;
};

// Terrain heigth map
RWTexture2D<float> OutputTexture : register(u0);

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
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// https://www.youtube.com/c/InigoQuilez
// https://iquilezles.org
// Code from shadertoy: www.shadertoy.com/view/Msf3WH

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

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y) return;

    // -1, 1 range
    float2 coords = float2(id.xy) / float2(textureSize) * 2.0f - 1.0f;
    float distance = max(abs(coords.x), abs(coords.y));
    float falloff = smoothstep(0.0f, 1.0f, distance);

    float2 positionInChunk = (chunkPosition + float2(id.xy)) + float2(0.1f);

    float noise = 0.0f;

    float amp = amplitude;
    float freq = frequency;

    // Layer the noise at different scales FBM
    for (int i = 0; i < octaves; i++) {
        noise += amp * evaluateNoise(positionInChunk * freq);
        amp *= amplitudeScalar;
        freq *= frequencyScalar;
    }

    // Subtract the falloff value from the noise to achieve an island like height map
    noise = noise - falloff;

    // Output into texture
    OutputTexture[id.xy] = noise;
}
