#include "OceanCommon.slang"

cbuffer RootConsant : register(b0)
{
    float time;
    uint oceanSize;
    uint h0TextureView;
}

Texture2D<float4> textures[4096] : register(t0);

RWTexture2D<float4> DisplacementTexture : register(u0);
RWTexture2D<float4> SlopeTexture : register(u1);

// https://www.tutorialspoint.com/cplusplus-program-to-perform-complex-number-multiplication
float2 ComplexMult(float2 a, float2 b) {
    return float2(
        a.x * b.x - a.y * b.y,
        a.x * b.y + a.y * b.x
    );
}

float4 Permute(float4 data, float2 id) {
    return data * (1.0f - 2.0f * ((id.x + id.y) % 2));
}

float2 EulersFormula(float x) { return float2(cos(x), sin(x)); }

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    // Fetch amplitude signals
    float4 spectralSpectrumSignal = textures[h0TextureView][id.xy];
    float2 h0 = spectralSpectrumSignal.xy;
    float2 h0conj = spectralSpectrumSignal.zw;

    float halfN = oceanSize / 2.0f;

    // Create vector K, higher K values means shorter waves
    float deltaK = 2.0f * PI / (oceanSize / 4);
    float2 K = ((float2)id.xy - halfN) * deltaK;

    float kMag = length(K);
    float kMagRcp = rcp(kMag);

    if (kMag < 0.0001f) 
        kMagRcp = 1.0f;

    float w_0 = 2.0f * PI / 200.0f; // frequency loop
    // Wave frequency speed
    float dispersion = floor(sqrt(GRAVITY * kMag) / w_0) * w_0 * time;

    float2 exponent = EulersFormula(dispersion);

    float2 htilde = ComplexMult(h0, exponent) + ComplexMult(h0conj, float2(exponent.x, -exponent.y));
    float2 ih = float2(-htilde.y, htilde.x);

    float2 displaceX = ih * K.x * kMagRcp;
    float2 displaceY = htilde;
    float2 displaceZ = ih * K.y * kMagRcp;

    float2 displaceXdx = -htilde * K.x * K.x * kMagRcp;
    float2 displaceYdx = ih * K.x;
    float2 displaceZdx = -htilde * K.x * K.y * kMagRcp;

    float2 displaceYdz = ih * K.y;
    float2 displaceZdz = -htilde * K.y * K.y * kMagRcp;

    float2 hDisplacementX = float2(displaceX.x - displaceZ.y, displaceX.y + displaceZ.x);
    float2 hDisplacementZ = float2(displaceY.x - displaceZdx.y, displaceY.y + displaceZdx.x);

    float2 hSlopeX = float2(displaceYdx.x - displaceYdz.y, displaceYdx.y + displaceYdz.x);
    float2 hSlopeZ = float2(displaceXdx.x - displaceZdz.y, displaceXdx.y + displaceZdz.x);

    // Store data in displacement and slope texture
    DisplacementTexture[id.xy] = float4(hDisplacementX, hDisplacementZ);
    SlopeTexture[id.xy] = float4(hSlopeX, hSlopeZ);
}