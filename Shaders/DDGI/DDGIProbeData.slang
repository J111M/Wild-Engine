
static const uint DDGI_MAX_CASCADES = 4;

struct DDGICascadeData
{
    int4 originCell;    // xyz = history volume min corner in cells, w = ray buffer slot or -1
    int4 newOriginCell; // xyz = volume min corner written this frame
    float4 spacing;     // x = spacing, y = distance update max ray distance
};

struct DDGIProbeData
{
    float4 randomRotation;

    int4 probeCounts; // xyz = counts per cascade, w = max rays per probe

    uint raysPerProbe;
    float maxRayDistance;
    float intensity;
    float hysteresis;

    uint irradianceView;
    uint distanceView;
    float normalBias;
    float viewBias;

    uint cascadeCount;
    uint tracedCascadeCount;
    float cascadeBlendCells;
    uint probeDataPadding;

    uint4 tracedCascades; // Cascade index per ray buffer slot

    DDGICascadeData cascades[DDGI_MAX_CASCADES];

    uint GetMaxRaysPerProbe() { return uint(probeCounts.w); }

    uint GetProbesPerCascade() { return uint(probeCounts.x * probeCounts.y * probeCounts.z); }

    float GetSpacing(uint cascade) { return cascades[cascade].spacing.x; }

    int3 GetStorageCoord(uint probeIndex)
    {
        uint countX = uint(probeCounts.x);
        uint countY = uint(probeCounts.y);
        return int3(probeIndex % countX, (probeIndex / countX) % countY, probeIndex / (countX * countY));
    }

    uint GetStorageIndex(int3 storageCoord)
    {
        return uint(storageCoord.x + storageCoord.y * probeCounts.x + storageCoord.z * probeCounts.x * probeCounts.y);
    }

    // World cells always map to the same slot
    int3 GetStorageFromWorldCell(int3 worldCell)
    {
        return ((worldCell % probeCounts.xyz) + probeCounts.xyz) % probeCounts.xyz;
    }

    int3 GetWorldCellFromStorage(int3 storageCoord, int3 originCell)
    {
        int3 local = (((storageCoord - originCell) % probeCounts.xyz) + probeCounts.xyz) % probeCounts.xyz;
        return originCell + local;
    }

    // Cascades are stacked as groups of probeCounts.y slices
    uint3 GetAtlasCoord(uint cascade, int3 storageCoord)
    {
        return uint3(storageCoord.x, cascade * uint(probeCounts.y) + uint(storageCoord.y), storageCoord.z);
    }
};

DDGIProbeData GetDDGIProbeData(uint probeDataView)
{
    return *DescriptorHandle<ConstantBuffer<DDGIProbeData>>(probeDataView);
}
