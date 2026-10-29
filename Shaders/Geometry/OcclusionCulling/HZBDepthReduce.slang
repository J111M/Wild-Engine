cbuffer RootConstant : register(b0)
{
    uint2 srcSize;
    uint2 dstSize;
}

Texture2D<float> Depth : register(t0);
RWTexture2D<float> OutMip : register(u0);

// Builds HZB mip 0, which is the power of two below the screen size so every mip after it exactly halves
[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{
    if (threadID.x >= dstSize.x || threadID.y >= dstSize.y) return;

    // A texel covers 1 to 2 depth pixels per axis, partially touched pixels count as well so the result stays conservative
    const uint2 first = (threadID.xy * srcSize) / dstSize;
    const uint2 last = min(((threadID.xy + 1) * srcSize + dstSize - 1) / dstSize, srcSize) - 1;

    float farthest = 0.0f;
    for (uint y = first.y; y <= last.y; y++)
    {
        for (uint x = first.x; x <= last.x; x++)
        {
            farthest = max(farthest, Depth[uint2(x, y)]);
        }
    }

    OutMip[threadID.xy] = farthest;
}
