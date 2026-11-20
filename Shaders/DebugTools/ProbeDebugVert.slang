#include "Helper/SceneData.slang"

import DDGI.DDGIProbeData;

cbuffer RootConstant : register(b0)
{
    float probeScale; // Fraction of the probe spacing
    uint probeDataView;
    float irradianceExposure;
    int debugCascade; // -1 draws every cascade
};

ConstantBuffer<SceneCameraData> sceneCamera : register(b1);

static const uint SEGMENTS = 12;
static const uint RINGS = 8;

static const float PI = 3.14159265f;

static const float2 quadCorners[6] = {
    float2(0.0f, 0.0f), float2(1.0f, 1.0f), float2(1.0f, 0.0f),
    float2(0.0f, 0.0f), float2(0.0f, 1.0f), float2(1.0f, 1.0f),
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    nointerpolation uint instanceId : INSTANCE_ID;
};

[shader("vertex")]
VSOutput main(uint vertexId: SV_VertexID, uint instanceId: SV_InstanceID)
{
    uint quadId = vertexId / 6;
    uint cornerId = vertexId % 6;

    float2 uv = (float2(quadId % SEGMENTS, quadId / SEGMENTS) + quadCorners[cornerId]) / float2(SEGMENTS, RINGS);

    float phi = uv.x * 2.0f * PI;
    float theta = uv.y * PI;

    float3 direction = float3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

    DDGIProbeData probeData = GetDDGIProbeData(probeDataView);
    uint probesPerCascade = probeData.GetProbesPerCascade();

    uint cascade = debugCascade >= 0 ? uint(debugCascade) : instanceId / probesPerCascade;
    uint probeIndex = debugCascade >= 0 ? instanceId : instanceId % probesPerCascade;

    DDGICascadeData cascadeData = probeData.cascades[cascade];
    int3 worldCell = probeData.GetWorldCellFromStorage(probeData.GetStorageCoord(probeIndex), cascadeData.originCell.xyz);
    float spacing = cascadeData.spacing.x;
    float3 probePosition = float3(worldCell) * spacing;
    float3 worldPosition = probePosition + direction * (probeScale * spacing);

    VSOutput output;
    output.position = mul(sceneCamera.viewProjection, float4(worldPosition, 1.0f));

    // Probes covered by the finer cascade are hidden so the cascades don't overlap
    if (cascade > 0)
    {
        DDGICascadeData finer = probeData.cascades[cascade - 1];
        float3 finerRelative = probePosition / finer.spacing.x - float3(finer.originCell.xyz);
        if (all(finerRelative >= 0.0f) && all(finerRelative <= float3(probeData.probeCounts.xyz - 1)))
        {
            output.position = float4(0.0f, 0.0f, 2.0f, 1.0f);
        }
    }

    output.normal = direction;
    output.instanceId = cascade * probesPerCascade + probeIndex;
    return output;
}
