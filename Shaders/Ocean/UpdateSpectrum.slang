#include "OceanCommon.slang"

cbuffer RootConsant : register(b0)
{
    float4 lengthScale;

    float time;
    uint oceanSize;
    uint h0TextureView;
}

Texture3D<float4> textures[4096] : register(t0);

RWTexture3D<float4> spectrumTexture : register(u0);
//RWTexture2D<float4> SlopeTexture : register(u1);

// https://www.tutorialspoint.com/cplusplus-program-to-perform-complex-number-multiplication
float2 ComplexMult(float2 a, float2 b) {
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
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

    for (int i = 0; i < 4; ++i) {
        // Fetch amplitude signals
        float4 spectralSpectrumSignal = textures[h0TextureView][uint3(id.xy, i)];
        float2 h0 = spectralSpectrumSignal.xy;
        float2 h0conj = spectralSpectrumSignal.zw;

        float halfN = OCEAN_SIZE / 2.0f;
        float2 K = (id.xy - halfN) * 2.0f * PI / lengthScale[i];
        float kMag = length(K);
        float kMagRcp = rcp(kMag);

        if (kMag < 0.0001f) {
            kMagRcp = 1.0f;
        }

        float w_0 = 2.0f * PI / 200.0f;
        float dispersion = floor(sqrt(GRAVITY * kMag) / w_0) * w_0 * time;

        float2 exponent = EulersFormula(dispersion);

        float2 htilde = ComplexMult(h0, exponent) + ComplexMult(h0conj, float2(exponent.x, -exponent.y));
        float2 ih = float2(-htilde.y, htilde.x);

        float2 displacementX = ih * K.x * kMagRcp;
        float2 displacementY = htilde;
        float2 displacementZ = ih * K.y * kMagRcp;

        float2 displacementX_dx = -htilde * K.x * K.x * kMagRcp;
        float2 displacementY_dx = ih * K.x;
        float2 displacementZ_dx = -htilde * K.x * K.y * kMagRcp;

        float2 displacementY_dz = ih * K.y;
        float2 displacementZ_dz = -htilde * K.y * K.y * kMagRcp;

        float2 htildeDisplacementX = float2(displacementX.x - displacementZ.y, displacementX.y + displacementZ.x);
        float2 htildeDisplacementZ = float2(displacementY.x - displacementZ_dx.y, displacementY.y + displacementZ_dx.x);
        
        float2 htildeSlopeX = float2(displacementY_dx.x - displacementY_dz.y, displacementY_dx.y + displacementY_dz.x);
        float2 htildeSlopeZ = float2(displacementX_dx.x - displacementZ_dz.y, displacementX_dx.y + displacementZ_dz.x);

        // Store data in displacement and slope texture
        spectrumTexture[uint3(id.xy, i * 2)] = float4(htildeDisplacementX, htildeDisplacementZ);
        spectrumTexture[uint3(id.xy, i * 2 + 1)] = float4(htildeSlopeX, htildeSlopeZ);
    }
}