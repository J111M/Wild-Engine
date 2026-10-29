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

    // Mip 0 is a power of two so each texel covers exactly a 2x2 block of its parent.
    // Coordinates are clamped for parents that are 1 texel wide or high.
    OutMip[threadID.xy] = max(max(SrcMip[min(base + uint2(0, 0), maxCoord)], SrcMip[min(base + uint2(1, 0), maxCoord)]),
                              max(SrcMip[min(base + uint2(0, 1), maxCoord)], SrcMip[min(base + uint2(1, 1), maxCoord)]));
}
