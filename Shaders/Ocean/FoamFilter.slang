#include "OceanCommon.slang"

RWTexture3D<float4> DisplacementTexture : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    // Box blur
    for (int i = 0; i < 4; ++i) {
        float foamSum = 0.0;
        int radius = 1;
        int count = 0;

        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                uint2 coord = uint2(
                    clamp(int(id.x) + dx, 0, int(OCEAN_SIZE) - 1),
                    clamp(int(id.y) + dy, 0, int(OCEAN_SIZE) - 1)
                );
                foamSum += DisplacementTexture[uint3(coord, i)].w;
                count++;
            }
        }

        float3 displacement = DisplacementTexture[uint3(id.xy, i)].xyz;

        DisplacementTexture[uint3(id.xy, i)] = float4(displacement, foamSum / float(count));
    }
}
