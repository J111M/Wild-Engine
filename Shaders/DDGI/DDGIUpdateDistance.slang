import DDGI.RayTracingUtils;
import DDGI.DDGIUpdateConstants;
import Helper.Common;

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

StructuredBuffer<ProbeRayData> probeRayData : register(t0);

#define CACHE_SIZE PROBE_DISTANCE_TEXELS * PROBE_DISTANCE_TEXELS
groupshared float SharedDepthCache[CACHE_SIZE];
groupshared float3 SharedDirectionCache[CACHE_SIZE];

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

static const uint BORDER_TEXELS = PROBE_DISTANCE_TEXELS * 4 + 4;
static const uint4 BORDER_OFFSETS[BORDER_TEXELS] = 
{
    uint4(14, 14, 0, 0), 
	uint4(14, 1, 1, 0),   uint4(13, 1, 2, 0),   uint4(12, 1, 3, 0),   uint4(11, 1, 4, 0), uint4(10, 1, 5, 0),   uint4(9, 1, 6, 0),    uint4(8, 1, 7, 0), 
	uint4(7, 1, 8, 0),    uint4(6, 1, 9, 0), uint4(5, 1, 10, 0),   uint4(4, 1, 11, 0),   uint4(3, 1, 12, 0),   uint4(2, 1, 13, 0),   uint4(1, 1, 14, 0),
    uint4(1, 14, 15, 0),
	uint4(1, 14, 0, 1),   uint4(1, 13, 0, 2),   uint4(1, 12, 0, 3),   uint4(1, 11, 0, 4), uint4(1, 10, 0, 5),   uint4(1, 9, 0, 6),    uint4(1, 8, 0, 7), 
	uint4(1, 7, 0, 8),    uint4(1, 6, 0, 9), uint4(1, 5, 0, 10),   uint4(1, 4, 0, 11),   uint4(1, 3, 0, 12),   uint4(1, 2, 0, 13),   uint4(1, 1, 0, 14),

    uint4(14, 1, 0, 15), 
	uint4(14, 14, 15, 1), uint4(14, 13, 15, 2), uint4(14, 12, 15, 3), uint4(14, 11, 15, 4), uint4(14, 10, 15, 5), uint4(14, 9, 15, 6),  uint4(14, 8, 15, 7), 
	uint4(14, 7, 15, 8),  uint4(14, 6, 15, 9), uint4(14, 5, 15, 10), uint4(14, 4, 15, 11), uint4(14, 3, 15, 12), uint4(14, 2, 15, 13), uint4(14, 1, 15, 14),

    uint4(1, 1, 15, 15), 
	uint4(14, 14, 1, 15), uint4(13, 14, 2, 15), uint4(12, 14, 3, 15), uint4(11, 14, 4, 15), uint4(10, 14, 5, 15), uint4(9, 14, 6, 15),  uint4(8, 14, 7, 15), 
	uint4(7, 14, 8, 15),  uint4(6, 14, 9, 15), uint4(5, 14, 10, 15), uint4(4, 14, 11, 15), uint4(3, 14, 12, 15), uint4(2, 14, 13, 15), uint4(1, 14, 14, 15),
};

[shader("compute")]
[numthreads(PROBE_DISTANCE_TEXELS, PROBE_DISTANCE_TEXELS, 1)]
void main(CSInput input) {
    uint3 probeGridCoords = uint3(input.GroupId.x, input.GroupId.z, input.GroupId.y);

    uint probeIndex = probeGridCoords.x + probeGridCoords.y * probeCounts.x +
                      probeGridCoords.z * probeCounts.x * probeCounts.y;

    uint3 texelLocation = GetProbeTexel(probeGridCoords, PROBE_DISTANCE_TEXELS);
    uint3 cornerTexelLocation = texelLocation - uint3(1, 1, 0);
    texelLocation.xy += input.GroupThreadId.xy;

    // Two channels, the mean hit distance and the mean of its square, matching the R32G32_FLOAT atlas
    Texture2DArray<float2> distanceHistory = DescriptorHandle<Texture2DArray<float2>>(distanceReadView);
    RWTexture2DArray<float2> newDistance = DescriptorHandle<RWTexture2DArray<float2>>(distanceWriteView);

    float2 prevDistance = distanceHistory[texelLocation];

    float3 probeDirection = OctNormalDecode(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_DISTANCE_TEXELS) * 2 - 1);

    float weightedSum = 0;
    float2 result = 0;

    uint remainingRays = raysPerProbe;
    uint offset = 0;

    while (remainingRays > 0)
    {
        uint numRays = min(CACHE_SIZE, remainingRays);

        // Wait for the previous batch to be consumed before overwriting the caches.
        GroupMemoryBarrierWithGroupSync();

        if (input.GroupIndex < numRays)
        {
            uint rayIndex = offset + input.GroupIndex;

            SharedDepthCache[input.GroupIndex] = probeRayData[probeIndex * GetMaxRaysPerProbe() + rayIndex].hitDistance;
            // Rebuilding the raydirection to be the same as the initial trace rotation
            SharedDirectionCache[input.GroupIndex] = GetRayDirection(rayIndex, raysPerProbe, randomRotation);
        }

        GroupMemoryBarrierWithGroupSync();

        // Accumulating the weight of each cached radiance
        for (uint i = 0; i < numRays; ++i)
        {
            float depth = min(abs(SharedDepthCache[i]), probeMaxRayDistance);
            float3 direction = SharedDirectionCache[i];
            float weight = saturate(dot(probeDirection, direction));
            weight = pow(weight, 64);

            if (weight > MIN_WEIGHT)
            {
                weightedSum += weight;
                result += float2(depth, Pow2(depth)) * weight;
            }
        }

        remainingRays -= numRays;
        offset += numRays;
    }

    const float epsilon = 1e-9f * float(raysPerProbe);
    result *= 1.0f / max(2.0f * weightedSum, epsilon);

    // Get blend weight
    const float historyBlendWeight = saturate(1.0f - hysteresis);
    result = lerp(prevDistance, result, historyBlendWeight);

    newDistance[texelLocation] = result;

    // Orders the UAV writes above against the UAV reads below, a group memory barrier only covers groupshared.
    AllMemoryBarrierWithGroupSync();

    for (uint index = input.GroupIndex; index < BORDER_TEXELS; index += PROBE_DISTANCE_TEXELS * PROBE_DISTANCE_TEXELS)
    {
        // BORDER_OFFSETS are corner relative in plane offsets, the corner's slice carries through untouched
        uint3 sourceIndex = cornerTexelLocation + uint3(BORDER_OFFSETS[index].xy, 0);
        uint3 targetIndex = cornerTexelLocation + uint3(BORDER_OFFSETS[index].zw, 0);
        newDistance[targetIndex] = newDistance[sourceIndex];
    }
}
