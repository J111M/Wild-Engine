#include "Helper/Common.slang"

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    uint vertexId : SV_VertexID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    nointerpolation uint meshletId : MESHLETID;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 invTransposeModel;
    uint albedoView;
    uint normalView;
    uint roughnessMetallicView;
    uint emissiveView;
    uint vertexMeshletIdBufferView;
    uint meshletDebugView;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(model, float4(input.position, 1.0f));
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    output.tangent = input.tangent;

    // Temporary debug for meshlets will be removed later
    output.meshletId = MESHLET_DEBUG_DISABLED;
    if (meshletDebugView != 0 && vertexMeshletIdBufferView != 0)
    {
        StructuredBuffer<uint> vertexMeshletIds = DescriptorHandle<StructuredBuffer<uint>>(vertexMeshletIdBufferView);
        output.meshletId = vertexMeshletIds[input.vertexId];
    }

    return output;
}
