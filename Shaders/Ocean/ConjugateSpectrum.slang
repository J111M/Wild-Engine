#include "../Helper/Common.slang"
#include "../Helper/Rng.slang"
#include "OceanCommon.slang"
// Implementation referenced from https://github.com/gasgiant/FFT-Ocean/
// Initial spectrum implementation is based on it

struct SpectrumSettings {
    float scale;
    float angle;
    float spreadBlend;
    float swell;

    float alpha;
    float peakOmega;
    float gamma;
    float shortWavesFade;
}

cbuffer RootConsant : register(b0)
{
    SpectrumSettings spectrum0;
    SpectrumSettings spectrum1;

    uint4 legthScale;

    float oceanDepth;

    uint oceanSize;
}

RWTexture3D<float4> OutputTexture : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    for (int i = 0; i < 4; ++i) {
        // Get origional h0 variables
        float2 h0 = OutputTexture[uint3(id.xy, i)].rg;
        float2 h0conj = OutputTexture[uint3(uint2((OCEAN_SIZE - id.x) % OCEAN_SIZE, (OCEAN_SIZE - id.y) % OCEAN_SIZE), i)].rg;

        // h0 Conjugate
        OutputTexture[uint3(id.xy, i)] = float4(h0, h0conj.x, -h0conj.y);
    }
   
}
