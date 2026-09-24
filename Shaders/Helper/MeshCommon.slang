const static uint TASK_SHADER_DISPATCH_X = 64;


struct MeshletPayload {
    uint32_t groupMeshletOffset;
    uint32_t meshletIndices[TASK_SHADER_DISPATCH_X];
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
    uint meshletCount;
};
