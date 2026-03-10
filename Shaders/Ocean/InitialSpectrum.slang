#include "OceanCommon.slang"
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

struct ComplexValue
{
    float real;
    float imaginary;
}

StructuredBuffer<ComplexValue> complexValues : register(t0);

RWTexture2D<float4> OutputTexture : register(u0);

#define SIGMA_L 0.07
#define SIGMA_R 0.09

float2 ComplexMult(float2 a, float2 b) { return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

float2 EulerFormula(float x) { return float2(cos(x), sin(x)); }

// Transfer momentum approximination formula
float TMACorrection(float omega)
{
    float omegaH = omega * sqrt(oceanDepth / GRAVITY);
    if (omegaH <= 1.0f) return 0.5f * omegaH * omegaH;
    if (omegaH < 2.0f) return 1.0f - 0.5f * (2.0f - omegaH) * (2.0f - omegaH);

    return 1.0f;
}

float NormalizationFactor(float s) {
    float s2 = s * s;
    float s3 = s2 * s;
    float s4 = s3 * s;
    if (s < 5) return -0.000564f * s4 + 0.00776f * s3 - 0.044f * s2 + 0.192f * s + 0.163f;
    else return -4.80e-08f * s4 + 1.07e-05f * s3 - 9.53e-04f * s2 + 5.90e-02f * s + 3.93e-01f;
}

float Cosine2s(float theta, float s)
{
	return NormalizationFactor(s) * pow(abs(cos(0.5 * theta)), 2 * s);
}

float SpreadPower(float omega)
{
    if (omega > peakOmega)
        return 9.77 * pow(abs(omega / peakOmega), -2.5);
    else
        return 6.97 * pow(abs(omega / peakOmega), 5);
}

float WaveDirectionSpectrum(float theta, float omega)
{
    float s = SpreadPower(omega)
		+ 16 * tanh(min(omega / peakOmega, 20)) * swell * swell;
    return lerp(2 / 3.1415 * cos(theta) * cos(theta), Cosine2s(theta - angle, s), spreadBlend);
}

// returns the frequency domain of the JONSWAP wave energy spectrum
float JONSWAPSpectrum(float omega)
{
    float sigma = (omega <= peakOmega) ? SIGMA_L : SIGMA_R;

    float r = exp(-(omega - peakOmega) * (omega - peakOmega) / 2.0f / sigma / sigma / peakOmega / peakOmega);

    float oneOverOmega = 1.0f / omega;
    float peakOmegaOverOmega = peakOmega / omega;

    // Spectral energy
    return scale * TMACorrection(omega) * alpha * GRAVITY * GRAVITY * pow(oneOverOmega, 5) *
        exp(-1.25f * pow(peakOmegaOverOmega, 4)) * pow(abs(gamma), r);
}

// The time it takes for small waves to fade
float ShortWavesFade(float kLength)
{
    // Exponential fade
    return exp(-shortWavesFade * shortWavesFade * kLength * kLength);
}

// Calculate Omega
float AngularFrequency(float k) { return sqrt(GRAVITY * k * tanh(min(k * oceanDepth, 20))); }

// (dw/dk)
float GroupVelocity(float k)
{
    float th = tanh(min(k * oceanDepth, 20));
    float ch = cosh(k * oceanDepth);
    return GRAVITY * (oceanDepth * k / ch / ch + th) / AngularFrequency(k) / 2;
}

// Compute h0(k)
float2 ComputeH0(float2 k, uint2 id)
{
    float kLen = length(k);

    ComplexValue val = complexValues[id.y * oceanSize + id.x];

    float deltaK = 2.0f * PI / (oceanSize / 4);

    if (0.0001f <= kLen && kLen <= 9000.0f)
    {
        float kAngle = atan2(k.y, k.x);

        // Calculate wave energy dispersion
        float omega = AngularFrequency(kLen);
        float dOmegadk = GroupVelocity(kLen);

        float spectralEnergySpectrum = JONSWAPSpectrum(omega) * WaveDirectionSpectrum(kAngle, omega) * ShortWavesFade(kLen);

        return float2(val.real, val.imaginary) * sqrt(2 * spectralEnergySpectrum * abs(dOmegadk) / kLen * deltaK * deltaK);
    }

    return float2(0.0f);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    float halfN = oceanSize / 2.0f;

    float deltaK = 2.0f * PI / (oceanSize / 4);

    float2 K = ((float2)id.xy - halfN) * deltaK;

    // K wave vector
    float2 h0 = ComputeH0(K, id.xy);

    // h0(k)
    OutputTexture[id.xy] = float4(h0, 0, 1);
}
