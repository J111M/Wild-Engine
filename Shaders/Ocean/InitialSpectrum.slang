#include "../Helper/Rng.slang"
#include "OceanCommon.slang"
// Implementation reference https://github.com/gasgiant/FFT-Ocean/

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

    uint4 lengthScale;

    float oceanDepth;
    uint oceanSize;
}

struct ComplexValue
{
    float real;
    float imaginary;
}

StructuredBuffer<ComplexValue> complexValues : register(t0);

RWTexture3D<float4> OutputTexture : register(u0);

#define SIGMA_L 0.07
#define SIGMA_R 0.09

//float2 ComplexMult(float2 a, float2 b) { return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }

float2 EulerFormula(float x) { return float2(cos(x), sin(x)); }

float hash(uint n) {
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 0x789221U) + 0x76312589U; // truncated to 32-bit
    return float(n & uint(0x7fffffffU)) / float(0x7fffffff);
}


// Transfer momentum approximination formula
float TMACorrection(float omega)
{
    float omegaH = omega * sqrt(oceanDepth / GRAVITY);
    if (omegaH <= 1.0f)
	    return 0.5f * omegaH * omegaH;
    if (omegaH < 2.0f)
	    return 1.0f - 0.5f * (2.0f - omegaH) * (2.0f - omegaH);

	return 1.0f;
}

float NormalizationFactor(float s)
{
    float s2 = s * s;
    float s3 = s2 * s;
    float s4 = s3 * s;
    if (s < 5) return -0.000564f * s4 + 0.00776f * s3 - 0.044f * s2 + 0.192f * s + 0.163f;
    else return -4.80e-08f * s4 + 1.07e-05f * s3 - 9.53e-04f * s2 + 5.90e-02f * s + 3.93e-01f;
}

float Cosine2s(float theta, float s) { return NormalizationFactor(s) * pow(abs(cos(0.5f * theta)), 2.0f * s); }

float SpreadPower(float omega, float peakOmega)
{
    if (omega > peakOmega)
		return 9.77f * pow(abs(omega / peakOmega), -2.5f);
	else
		return 6.97f * pow(abs(omega / peakOmega), 5.0f);
}

float WaveDirectionSpectrum(float theta, float omega, SpectrumSettings settings)
{
    float s = SpreadPower(omega, settings.peakOmega) + 16 * tanh(min(omega / settings.peakOmega, 20)) * settings.swell * settings.swell;
    return lerp(2.0f / 3.1415 * cos(theta) * cos(theta), Cosine2s(theta - settings.angle, s), settings.spreadBlend);
}

// returns the frequency domain of the JONSWAP wave energy spectrum
float JONSWAPSpectrum(float omega, SpectrumSettings settings)
{
    float sigma = (omega <= settings.peakOmega) ? 0.07f : 0.09f;

    float r = exp(-(omega - settings.peakOmega) * (omega - settings.peakOmega) / 2.0f / sigma / sigma / settings.peakOmega / settings.peakOmega);

    float oneOverOmega = 1.0f / omega;
    float peakOmegaOverOmega = settings.peakOmega / omega;
    // Spectral energy output
    return (settings.scale * 100000.0f) * TMACorrection(omega) * settings.alpha * GRAVITY * GRAVITY
		* oneOverOmega * oneOverOmega * oneOverOmega * oneOverOmega * oneOverOmega
		* exp(-1.25f * peakOmegaOverOmega * peakOmegaOverOmega * peakOmegaOverOmega * peakOmegaOverOmega)
		* pow(abs(settings.gamma), r);
}

// The time it takes for small waves to fade
float ShortWavesFade(float kLength, SpectrumSettings settings)
{
    // Exponential fade
    return exp(-settings.shortWavesFade * settings.shortWavesFade * kLength * kLength);
}

// Calculate Omega
float AngularFrequency(float kLength) { return sqrt(GRAVITY * kLength * tanh(min(kLength * oceanDepth, 20))); }

// (dw/dk)
float GroupVelocity(float kLength)
{
    float th = tanh(min(kLength * oceanDepth, 20));
    float ch = cosh(kLength * oceanDepth);
    return GRAVITY * (oceanDepth * kLength / ch / ch + th) / AngularFrequency(kLength) / 2.0f;
}

float2 BoxMuller(float u1, float u2)
{
    float R = sqrt(-2.0f * log(u1));
    float theta = 2.0f * PI * u2;

    return float2(R * cos(theta), R * sin(theta));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    uint seed = id.x + oceanSize * id.y + oceanSize;
    seed += 10;

    float halfN = oceanSize / 2.0f;

    for (int i = 0; i < 4; ++i)
    {
        // Compute h0(k), k is the wave vector
        float2 h0 = float2(0.0f);

        float deltaK = 2.0f * PI / float(lengthScale[i]);
        float2 K = ((float2)id.xy - halfN) * deltaK;
        float kLen = length(K);

        // ComplexValue val = complexValues[id.y * oceanSize + id.x];
        seed += i + murmurHash11(seed) * 10;
        float4 uniformRandom = float4(uhash11(seed), uhash11(seed * 2), uhash11(seed * 3), uhash11(seed * 4));
        // Generate 2 sets of Gaussian ranadomized numbers to prevent the samples being the same
        float2 gauss1 = BoxMuller(uniformRandom.x, uniformRandom.y);
        float2 gauss2 = BoxMuller(uniformRandom.z, uniformRandom.w);

        if (0.001f <= kLen && kLen <= 9000.0f)
        {
            float kAngle = atan2(K.y, K.x);

            // Calculate wave energy dispersion
            float omega = AngularFrequency(kLen);
            float dOmegadk = GroupVelocity(kLen);

            float spectralEnergySpectrum = JONSWAPSpectrum(omega, spectrum0) * WaveDirectionSpectrum(kAngle, omega, spectrum0) * ShortWavesFade(kLen, spectrum0);

            if (spectrum1.scale > 0) {
                spectralEnergySpectrum += JONSWAPSpectrum(omega, spectrum1) * WaveDirectionSpectrum(kAngle, omega, spectrum1) * ShortWavesFade(kLen, spectrum1);
            }

            h0 = float2(gauss2.x, gauss1.y) * sqrt(2 * spectralEnergySpectrum * abs(dOmegadk) / kLen * deltaK * deltaK);
        }
        else{
            h0 = float2(0.0f,0.0f);
        }
        // h0(k)
        OutputTexture[uint3(id.xy, i)] = float4(h0, 0.0f, 0.0f);
    }
}