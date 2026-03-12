#include "OceanCommon.slang"

cbuffer RootConsant : register(b0)
{
    uint oceanSize;
    uint fourierTextureView;

    float foamBias;
    float foamDecayRate;
    float foamThreshold;
    float foamAdd;
}

Texture3D<float4> textures[4096] : register(t0);

RWTexture3D<float4> DisplacementTexture : register(u0);
RWTexture3D<float4> SlopeTexture : register(u1);

float4 Permute(float4 data, uint2 id) {
    return data * (1.0f - 2.0f * ((id.x + id.y) % 2));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= oceanSize || id.y >= oceanSize) return;

    for (int i = 0; i < 4; ++i) {
        float4 displace = Permute(textures[fourierTextureView][uint3(id.xy, i * 2)], id.xy);
        float4 slope = Permute(textures[fourierTextureView][uint3(id.xy, i * 2 + 1)], id.xy);

        float2 lambda = float2(1.0f, 1.0f);

        float jacobian = (1.0f + lambda.x * slope.b) * (1.0f + lambda.y * slope.a) - lambda.x * lambda.y * pow(slope.g, 2);

        float3 displacement = float3(lambda.x * displace.r, displace.b, lambda.y * displace.g);

        float2 slopes = slope.rg / (1 + abs(slope.ba * lambda));

        float foam = displace.a;
        foam *= exp(-foamDecayRate);
        foam = saturate(foam);

        float biasedJacobian = max(0.0f, -(jacobian - foamBias));

        if (biasedJacobian > foamThreshold)
            foam += foamAdd * biasedJacobian;

        DisplacementTexture[uint3(id.xy, i)] = float4(displacement, foam);
        SlopeTexture[uint3(id.xy, i)] = float4(slopes, 0, 1);
    }
}
