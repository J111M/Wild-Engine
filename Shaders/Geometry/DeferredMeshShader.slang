#include "Helper/Common.slang"

const static uint MESH_SHADER_DISPATCH_X = 32;
const static uint MAX_VERTICES = 64;
const static uint MAX_PRIMITIVES = 124;

struct Vertex
{
    float3 position;
    float positionPadding;
    float3 color;
    float colorPadding;
    float3 normal;
    float normalPadding;
    float2 uv;
    float2 uvPadding;
    float4 tangent;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    nointerpolation uint meshletId : MESHLETID;
};

struct Meshlet
{
    float4 meshletBoundingSphere;
    float3 coneApex;
    float coneCutoff;
    float3 coneAxis;
    uint vertexOffset;
    uint meshletVerticesOffset;
    uint meshletTriangleOffset;
    uint meshletVerticesCount;
    uint meshletTriangleCount;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 invTransposeModel;
    uint albedoView;
    uint normalView;
    uint roughnessMetallicView;
    uint emissiveView;
    uint vertexDataBuffer;
    uint meshletBufferView;
    uint meshletVertexBufferView;
    uint primIndexBufferView;
    uint meshletOffset;
    uint meshletDebugView;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESH_SHADER_DISPATCH_X, 1, 1)]
void main(OutputIndices<uint3, MAX_PRIMITIVES> triangles, OutputVertices<VertexOutput, MAX_VERTICES> vertices,
          uint3 groupId : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    StructuredBuffer<Vertex> vertexBuffer = DescriptorHandle<StructuredBuffer<Vertex>>(vertexDataBuffer);
    StructuredBuffer<Meshlet> meshlets = DescriptorHandle<StructuredBuffer<Meshlet>>(meshletBufferView);
    StructuredBuffer<uint> vertexIndices = DescriptorHandle<StructuredBuffer<uint>>(meshletVertexBufferView);
    StructuredBuffer<uint> primitiveIndices = DescriptorHandle<StructuredBuffer<uint>>(primIndexBufferView);

    uint meshletId = meshletOffset + groupId.x;
    Meshlet meshlet = meshlets[meshletId];
    SetMeshOutputCounts(meshlet.meshletVerticesCount, meshlet.meshletTriangleCount);

    for (uint i = gtid.x; i < meshlet.meshletVerticesCount; i += MESH_SHADER_DISPATCH_X)
    {
        uint vertexIndex = vertexIndices[meshlet.meshletVerticesOffset + i] + meshlet.vertexOffset;
        Vertex input = vertexBuffer[vertexIndex];
        VertexOutput output;
        output.position = mul(model, float4(input.position, 1.0f));
        output.color = input.color;
        output.normal = input.normal;
        output.uv = input.uv;
        output.tangent = input.tangent;
        output.meshletId = meshletDebugView != 0 ? meshletId : MESHLET_DEBUG_DISABLED;
        vertices[i] = output;
    }

    for (uint i = gtid.x; i < meshlet.meshletTriangleCount; i += MESH_SHADER_DISPATCH_X)
    {
        uint index = meshlet.meshletTriangleOffset + i * 3;
        triangles[i] = uint3(primitiveIndices[index], primitiveIndices[index + 1], primitiveIndices[index + 2]);
    }
}
