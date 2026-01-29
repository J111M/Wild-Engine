struct GrassBlade
{
    float3 worldPosition;
    float rotation; // in radians
    float height;
};

RWStructuredBuffer<GrassBlade> GrassData : register(u0);

cbuffer RootConsant : register(b0)
{
    float4x4 modelMatrix;
    float2 chunkPosition;
    float2 minMaxHeight;
    uint seed;
    uint chunkId;
};

// https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11
float HashFloat(uint x)
{
    x ^= x >> 16;
    x *= 0x21F0AAADu;
    x ^= x >> 15;
    x *= 0xD35A2D97u;
    x ^= x >> 15;
    return frac((float) x / 4294967296.0);
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

// Hash source shadertoy: https://www.shadertoy.com/view/ttc3zr
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

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // TODO add bounds for safity

    // 25000 blades per chunk
    const uint chunkOffset = chunkId * 25000;
    uint offsetId = chunkOffset + DTid.x;

    // TODO modify PerBladeData to match chunk and chunk id
    float2 rHash1 = HashFloat2InRange(offsetId + 1, 0, 128); // 128 units per chunk for now

    float3 bladePosition = float3(rHash1.x + chunkPosition.x, 0, rHash1.y + chunkPosition.y);

    GrassData[offsetId].worldPosition = bladePosition.xyz;

    float rHash2 = HashFloatInRange(offsetId ^ 0x6C8E9CF5u, 0, 16);

    GrassData[offsetId].rotation = rescale(uhash11(offsetId + 1), 0, 4) * 2 * 3.141592653589793f; // To radiance

    GrassData[offsetId].height = HashFloatInRange(offsetId + 500, 0.1, 1.1);

}