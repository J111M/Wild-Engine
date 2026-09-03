struct DDGIProbeData
{
    float4 randomRotation;

    float4 probeOrigin;
    float4 probeSpacing;
    int4 probeCounts;

    uint raysPerProbe;
    float maxRayDistance;
    float intensity;

    float hysteresis;

    uint irradianceView;
    uint distanceView;

    float normalBias;
    float viewBias;

    uint GetMaxRaysPerProbe() { return uint(probeCounts.w); }

    uint GetProbeCount() { return uint(probeCounts.x * probeCounts.y * probeCounts.z); }
};

DDGIProbeData GetDDGIProbeData(uint probeDataView)
{
    return *DescriptorHandle<ConstantBuffer<DDGIProbeData>>(probeDataView);
}
