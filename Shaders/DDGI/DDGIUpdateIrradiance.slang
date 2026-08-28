import DDGI.RayTracingUtils;
import DDGI.DDGIConstants;
import Helper.Common;

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

StructuredBuffer<ProbeRayData> probeRayData : register(t0);

// The irradiance pair ping-pongs, so both resources are resolved from the descriptor heap through the
// indices in DDGIConstants rather than through fixed register slots.

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

uint3 GetProbeGridCoord(uint probeIndex)
{
    uint3 gridCoord;
    gridCoord.x = probeIndex % probeCounts.x;
    gridCoord.y = (probeIndex % (probeCounts.x * probeCounts.y)) / probeCounts.x;
    gridCoord.z = probeIndex / (probeCounts.x * probeCounts.y);
    return gridCoord;
}

uint2 GetProbeTexel(uint3 probeIndex3D, uint numProbeInteriorTexels)
{
    uint numProbeTexels = 1 + numProbeInteriorTexels + 1;
    return uint2(probeIndex3D.x + probeIndex3D.y * probeCounts.x, probeIndex3D.z) * numProbeTexels + 1;
}

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
    uint probeIndex = input.GroupId.x;
    uint3 probeGridCoords = GetProbeGridCoord(probeIndex);
    uint2 texelLocation = GetProbeTexel(probeGridCoords, PROBE_IRRADIANCE_TEXELS);
    uint2 cornerTexelLocation = texelLocation - 1u;
    texelLocation += input.GroupThreadId.xy;

    // Check if the probe index doesnt exceed the count
    if (probeIndex >= GetProbeCount()) return;

    Texture2D<float4> irradianceSrc = DescriptorHandle<Texture2D<float4> >(irradianceReadView);
    RWTexture2D<float4> irradianceDst = DescriptorHandle<RWTexture2D<float4> >(irradianceWriteView);

    float3 prevRadiance = irradianceSrc[texelLocation].rgb;

    float3 probeDirection = OctNormalDecode(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_IRRADIANCE_TEXELS) * 2 - 1);

    float weightedSum = 0;
    float3 result = 0;

    // Rays are traced in batches of at most CACHE_SIZE so a group of PROBE_IRRADIANCE_TEXELS^2 threads can
    // cooperatively pull them into groupshared memory before every thread reads all of them.
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
            
            SharedRadianceCache[input.GroupIndex] = probeRayData[probeIndex * GetMaxRaysPerProbe() + rayIndex].radiance;
            // Rebuilding the raydirection to be the same as the initial trace rotation
            SharedDirectionCache[input.GroupIndex] = GetRayDirection(rayIndex, raysPerProbe, randomRotation);
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

        // TODO accumulate: weight each cached radiance by saturate(dot(texelDirection, SharedDirectionCache[i]))
        // and skip rays whose hitDistance is negative (the trace pass marks backface hits that way).

        remainingRays -= numRays;
        offset += numRays;
    }

    const float epsilon = 1e-9f * float(raysPerProbe);
    result *= 1.0f / max(2.0f * weightedSum, epsilon);

    result = pow(result, rcp(5.0f));

    // Get blend weight
    const float historyBlendWeight = saturate(1.0f - hysteresis);
    result = lerp(prevRadiance, result, historyBlendWeight);

    irradianceDst[texelLocation] = float4(result, 1);

    GroupMemoryBarrierWithGroupSync();

    for (uint index = input.GroupIndex; index < BORDER_TEXELS; index += PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS)
    {
        uint2 sourceIndex = cornerTexelLocation + BORDER_OFFSETS[index].xy;
        uint2 targetIndex = cornerTexelLocation + BORDER_OFFSETS[index].zw;
        irradianceDst[targetIndex] = irradianceDst[sourceIndex];
    }
}
