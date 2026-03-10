#include "../Helper/Common.slang"
#include "../Helper/Rng.slang"

// Implementation referenced from https://github.com/gasgiant/FFT-Ocean/
// Initial spectrum implementation is based on it

cbuffer RootConsant : register(b0)
{
    float scale;
    float angle;
    float spreadBlend;
    float swell;

    float alpha;
    float peakOmega;
    float gamma;
    float shortWavesFade;

    float oceanDepth;

    uint oceanSize;
}

RWTexture2D<float4> OutputTexture : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    // Get origional h0 variables
    float2 h0 = OutputTexture[id.xy].rg;
    float2 h0conj = OutputTexture[uint2((oceanSize - id.x) % oceanSize, (oceanSize - id.y) % oceanSize)].rg;

    // h0 Conjugate
    OutputTexture[id.xy] = float4(h0, h0conj.x, -h0conj.y);
}
