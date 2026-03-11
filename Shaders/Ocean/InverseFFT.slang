#include "OceanCommon.slang"

cbuffer RootConsant : register(b0)
{
    uint oceanSize;
    uint axisFlag;
    uint spectrumTextureView;
}

Texture3D<float4> textures[4096] : register(t0);

RWTexture3D<float4> fourierTarget : register(u0);

// https://www.tutorialspoint.com/cplusplus-program-to-perform-complex-number-multiplication
float2 ComplexMult(float2 a, float2 b) {
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}


void ButterflyValues(uint steps, uint index, out uint2 indices, out float2 twiddle) {
    const float twoPi = 6.28318530718;
    uint b = OCEAN_SIZE >> (steps + 1);
    uint w = b * (index / b);
    uint i = (w + index) % OCEAN_SIZE;
    sincos(-twoPi / OCEAN_SIZE * w, twiddle.y, twiddle.x); // 6.2831 = PI * 2

    // Inverse
    twiddle.y = -twiddle.y;
    indices = uint2(i, i + b);
}

groupshared float4 fftGroupBuffer[2][OCEAN_SIZE];

float4 FFT(uint threadIndex, float4 input) {
    fftGroupBuffer[0][threadIndex] = input;
    GroupMemoryBarrierWithGroupSync();
    bool flag = false;

    [unroll]
    for (uint step = 0; step < LOG_SIZE; ++step) {
        uint2 inputsIndices;
        float2 twiddle;
        ButterflyValues(step, threadIndex, inputsIndices, twiddle);

        float4 v = fftGroupBuffer[flag][inputsIndices.y];
        fftGroupBuffer[!flag][threadIndex] = fftGroupBuffer[flag][inputsIndices.x] + float4(ComplexMult(twiddle, v.xy), ComplexMult(twiddle, v.zw));

        flag = !flag;
        GroupMemoryBarrierWithGroupSync();
    }

    return fftGroupBuffer[flag][threadIndex] / OCEAN_SIZE;
}

[shader("compute")]
[numthreads(OCEAN_SIZE, 1, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    for (int i = 0; i < 8; ++i) {
        GroupMemoryBarrierWithGroupSync();
        if (axisFlag == 0) {
            fourierTarget[uint3(id.xy, i)] = FFT(id.x, fourierTarget[uint3(id.xy, i)]);
        } else if (axisFlag >= 1) {
            fourierTarget[uint3(id.yx, i)] = FFT(id.x, fourierTarget[uint3(id.yx, i)]);
        }
    }
}