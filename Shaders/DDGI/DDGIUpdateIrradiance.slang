import DDGI.RaytracingUtils;

cbuffer BlendConstants : register(b0) {
    float hysteresis;
    uint raysPerProbe;
    uint maxRaysPerProbe;
    uint probeCount;
};

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

StructuredBuffer<ProbeRayData> probeRayData : register(t0);
RWStructuredBuffer<float4> probeIrradiance : register(u0);

cbuffer DDGIConstants : register(b0) {
    column_major float4x4 inverseView;

    float4 randomRotation; // quaternion (xyz axis*sin(half), w = cos(half))

    float4 lightDirection;      // xyz = direction the light travels, w unused
    float4 lightColorIntensity; // rgb = color, a = intensity

    float4 probeOrigin;  // xyz = grid origin, w unused
    float4 probeSpacing; // xyz = grid spacing, w unused
    int4 probeCounts;    // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

    uint raysPerProbe;
    float maxRayDistance;
    float intensity;
    uint environmentView;

    uint numPointLights;
    uint IrradianceView;
    uint DistanceView;
};

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

[shader("compute")]
[numthreads(PROBE_IRRADIANCE_TEXELS, PROBE_IRRADIANCE_TEXELS, 1)]
void main(CSInput input) {
    uint probeIndex = input.GroupId.x;
    uint3 probeGridCoords = GetProbeGridCoord(probeIndex);
    if (probeIndex >= probeCount) return;

    float3 sum = float3(0, 0, 0);
    uint count = 0;

    for (uint r = 0; r < raysPerProbe; r++) {
        ProbeRayData rayData = probeRayData[probeIndex * maxRaysPerProbe + r];

        // Negative hit distance marks a backface hit - discard it from the average.
        if (rayData.hitDistance < 0.0) continue;

        sum += rayData.radiance;
        count++;
    }

    float3 newIrradiance = (count > 0) ? (sum / float(count)) : float3(0, 0, 0);
    float3 oldIrradiance = probeIrradiance[probeIndex].rgb;

    float3 blended = lerp(oldIrradiance, newIrradiance, saturate(1.0 - hysteresis));
    probeIrradiance[probeIndex] = float4(blended, 1.0);
}
