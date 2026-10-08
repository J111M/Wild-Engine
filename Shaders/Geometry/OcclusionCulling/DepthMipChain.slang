cbuffer RootConstant : register(b0)
{
    uint2 srcSize;
    uint2 dstSize;
}

RWTexture2D<float> SrcMip : register(u0);
RWTexture2D<float> OutMip : register(u1);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{
    if (threadID.x >= dstSize.x || threadID.y >= dstSize.y) return;

    const uint2 maxCoord = srcSize - 1;
    const uint2 base = threadID.xy * 2;

    // Default case, both width and height are even so each texel covers a 2x2 block of its parent.
    // Coordinates are clamped for parents that are 1 texel wide or high.
    float src = max(max(SrcMip[min(base + uint2(0, 0), maxCoord)], SrcMip[min(base + uint2(1, 0), maxCoord)]),
                    max(SrcMip[min(base + uint2(0, 1), maxCoord)], SrcMip[min(base + uint2(1, 1), maxCoord)]));

    // If either the width or height is odd, the last texel in that direction also covers
    // Extra samples are necessary to prevent holes in the chain
    const bool extraColumn = (srcSize.x & 1) != 0 && threadID.x == dstSize.x - 1 && srcSize.x > 1;
    const bool extraRow = (srcSize.y & 1) != 0 && threadID.y == dstSize.y - 1 && srcSize.y > 1;

    if (extraColumn)
    {
        src = max(src, max(SrcMip[min(base + uint2(2, 0), maxCoord)], SrcMip[min(base + uint2(2, 1), maxCoord)]));
    }

    if (extraRow)
    {
        src = max(src, max(SrcMip[min(base + uint2(0, 2), maxCoord)], SrcMip[min(base + uint2(1, 2), maxCoord)]));
    }

    if (extraColumn && extraRow)
    {
        src = max(src, SrcMip[min(base + uint2(2, 2), maxCoord)]);
    }

    OutMip[threadID.xy] = src;
}
