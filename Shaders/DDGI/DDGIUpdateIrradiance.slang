import DDGI.RayTracingUtils;
import DDGI.DDGIProbeData;
import DDGI.DDGIUpdateConstants;
import Helper.Common;

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

StructuredBuffer<ProbeRayData> probeRayData : register(t0);

// The irradiance pair ping-pongs, so both resources are resolved from the descriptor heap through the
// indices in DDGIUpdateConstants rather than through fixed register slots.

#define CACHE_SIZE PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS
groupshared float3 SharedRadianceCache[CACHE_SIZE];
groupshared float3 SharedDirectionCache[CACHE_SIZE];

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

static const uint BORDER_TEXELS = PROBE_IRRADIANCE_TEXELS * 4 + 4;
static const uint4 BORDER_OFFSETS[BORDER_TEXELS] = 
{
	uint4(6, 6, 0, 0), 
	uint4(6, 1, 1, 0), uint4(5, 1, 2, 0), uint4(4, 1, 3, 0), uint4(3, 1, 4, 0), uint4(2, 1, 5, 0), uint4(1, 1, 6, 0),
	uint4(1, 6, 7, 0),
    uint4(1, 6, 0, 1), uint4(1, 5, 0, 2), uint4(1, 4, 0, 3), uint4(1, 3, 0, 4), uint4(1, 2, 0, 5), uint4(1, 1, 0, 6),
	uint4(6, 1, 0, 7),
	uint4(6, 6, 7, 1), uint4(6, 5, 7, 2), uint4(6, 4, 7, 3), uint4(6, 3, 7, 4), uint4(6, 2, 7, 5), uint4(6, 1, 7, 6),
	uint4(1, 1, 7, 7),
	uint4(6, 6, 1, 7), uint4(5, 6, 2, 7), uint4(4, 6, 3, 7), uint4(3, 6, 4, 7), uint4(2, 6, 5, 7), uint4(1, 6, 6, 7) 
};

[shader("compute")]
[numthreads(PROBE_IRRADIANCE_TEXELS, PROBE_IRRADIANCE_TEXELS, 1)]
void main(CSInput input) {
    DDGIProbeData probeData = GetDDGIProbeData(probeDataView);

    uint3 probeGridCoords = uint3(input.GroupId.x, input.GroupId.z, input.GroupId.y);

    uint probeIndex = probeGridCoords.x + probeGridCoords.y * probeData.probeCounts.x +
                      probeGridCoords.z * probeData.probeCounts.x * probeData.probeCounts.y;

    uint3 texelLocation = GetProbeTexel(probeGridCoords, PROBE_IRRADIANCE_TEXELS);
    uint3 cornerTexelLocation = texelLocation - uint3(1, 1, 0);
    texelLocation.xy += input.GroupThreadId.xy;

    Texture2DArray<float4> irradianceHistory = DescriptorHandle<Texture2DArray<float4>>(irradianceReadView);
    RWTexture2DArray<float4> newIrradiance = DescriptorHandle<RWTexture2DArray<float4>>(irradianceWriteView);

    float3 prevRadiance = irradianceHistory[texelLocation].rgb;
    bool invalidHistory = any(isnan(prevRadiance)) || any(isinf(prevRadiance)) || any(prevRadiance < 0.0f);
    prevRadiance = invalidHistory ? float3(0, 0, 0) : prevRadiance;

    float3 probeDirection = OctNormalDecode(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_IRRADIANCE_TEXELS) * 2 - 1);

    float weightedSum = 0;
    float3 result = 0;

    // Rays are traced in batches of at most CACHE_SIZE so a group of PROBE_IRRADIANCE_TEXELS^2 threads can
    // cooperatively pull them into groupshared memory before every thread reads all of them.
    uint remainingRays = probeData.raysPerProbe;
    uint offset = 0;

    while (remainingRays > 0)
    {
        uint numRays = min(CACHE_SIZE, remainingRays);

        // Wait for the previous batch to be consumed before overwriting the caches.
        GroupMemoryBarrierWithGroupSync();

        if (input.GroupIndex < numRays)
        {
            uint rayIndex = offset + input.GroupIndex;
            
            float3 rayRadiance = probeRayData[probeIndex * probeData.GetMaxRaysPerProbe() + rayIndex].radiance;
            bool invalidRay = any(isnan(rayRadiance)) || any(isinf(rayRadiance));
            SharedRadianceCache[input.GroupIndex] = invalidRay ? float3(0, 0, 0) : max(rayRadiance, float3(0, 0, 0));
            // Rebuilding the raydirection to be the same as the initial trace rotation
            SharedDirectionCache[input.GroupIndex] =
                GetRayDirection(rayIndex, probeData.raysPerProbe, probeData.randomRotation);
        }

        GroupMemoryBarrierWithGroupSync();

        // Accumulating the weight of each cached radiance
        for (uint i = 0; i < numRays; ++i)
        {
            float3 radiance = SharedRadianceCache[i].rgb;
            float3 direction = SharedDirectionCache[i];
            float weight = saturate(dot(probeDirection, direction));
            result += weight * radiance;
            weightedSum += weight;
        }

        remainingRays -= numRays;
        offset += numRays;
    }

    const float epsilon = 1e-9f * float(probeData.raysPerProbe);
    result *= 1.0f / max(2.0f * weightedSum, epsilon);

    result = pow(max(result, float3(0, 0, 0)), rcp(5.0f));

    const float historyBlendWeight = invalidHistory ? 1.0f : saturate(1.0f - probeData.hysteresis);
    result = lerp(prevRadiance, result, historyBlendWeight);

    newIrradiance[texelLocation] = float4(result, 1);

    // Orders the UAV writes above against the UAV reads below, a group memory barrier only covers groupshared.
    AllMemoryBarrierWithGroupSync();

    for (uint index = input.GroupIndex; index < BORDER_TEXELS; index += PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS)
    {
        // BORDER_OFFSETS are corner relative in plane offsets, the corner's slice carries through untouched
        uint3 sourceIndex = cornerTexelLocation + uint3(BORDER_OFFSETS[index].xy, 0);
        uint3 targetIndex = cornerTexelLocation + uint3(BORDER_OFFSETS[index].zw, 0);
        newIrradiance[targetIndex] = newIrradiance[sourceIndex];
    }
}
