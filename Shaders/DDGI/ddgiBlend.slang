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

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint probeIndex = id.x;
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
