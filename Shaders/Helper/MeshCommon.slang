#include "Helper/SceneData.slang"

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
    float4 baseColor; // Linear base colour
    uint vertexDataBuffer;
    uint meshletBufferView;
    uint meshletVertexBufferView;
    uint primIndexBufferView;
    float3 cullCameraPosition; // Object space position of the culling camera
    uint meshletCount;

    uint meshletOffset;
    uint meshletDebugView;

    uint instanceMeshletOffset; // First slot of this instance in the meshlet visibility buffer
    uint meshletVisibilityView; // Bindless UAV of the meshlet visibility buffer, INVALID_HEAP_INDEX when there is none

    uint hzbView; // Bindless SRV of the HZB, only used in the late pass
    uint2 hzbSize; // Size of HZB mip 0
    uint hzbMipCount;
};

ConstantBuffer<SceneCameraData> sceneCamera : register(b1);
