cbuffer RootConstant : register(b0)
{
    column_major float4x4 projView;
    float probeScale;
};

StructuredBuffer<float4> probePositions : register(t0);

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
    float3 worldPosition = probePositions[instanceId].xyz + direction * probeScale;

    VSOutput output;
    output.position = mul(projView, float4(worldPosition, 1.0f));
    output.normal = direction;
    output.instanceId = instanceId;
    return output;
}
