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
  
}
