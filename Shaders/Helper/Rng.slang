
// 1 input 1 output
float hash11(float src)
{
    uint h = murmurHash11(asuint(src));
    return asfloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}

float uhash11(uint src)
{
    uint h = murmurHash11(src);
    return asfloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}

float rescale(float value, float min, float max)
{
    return min + value * (max - min);
}

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

uint murmurHash12(uint2 src)
{
    const uint M = 0x5bd1e995u;
    uint h = 1190494759u;
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

float hash12(float2 p)
{
    uint h = murmurHash12(asuint(p));
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

float cellNoise(float2 p)
{
    float2 cell = floor(p);
    return hash22(cell).x;
}

// https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11
float HashFloat(uint x)
{
    x ^= x >> 16;
    x *= 0x21F0AAADu;
    x ^= x >> 15;
    x *= 0xD35A2D97u;
    x ^= x >> 15;
    return frac((float)x / 4294967296.0);
}

float HashFloatInRange(uint x, float minVal, float maxVal)
{
    float h = HashFloat(x);
    return minVal + h * (maxVal - minVal);
}

float2 HashFloat2InRange(uint seed, float minVal, float maxVal)
{
    float x = HashFloatInRange(seed, minVal, maxVal);
    float y = HashFloatInRange(seed ^ 0x6C8E9CF5u, minVal, maxVal);
    return float2(x, y);
}