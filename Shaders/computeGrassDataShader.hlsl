struct GrassBlade
{
    float3 position;
};

RWStructuredBuffer<GrassBlade> GrassData : register(u0);

// https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11
float HashFloat(uint x)
{
    x = (x ^ 61) ^ (x >> 16);
    x *= 9u;
    x = x ^ (x >> 4);
    x *= 0x27d4eb2du;
    x = x ^ (x >> 15);
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

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // TODO remove magic number
    //if (DTid.x >= 500)
    //    return;
    
    float2 randomHash = HashFloat2InRange(DTid.x + 1, 0, 32); // 32 units per chunk for now
    
    GrassData[DTid.x].position = float3(randomHash.x, 0, randomHash.y);
}