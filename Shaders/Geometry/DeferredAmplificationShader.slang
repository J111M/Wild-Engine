#include "Helper/MeshCommon.slang"

groupshared MeshletPayload sharedPayload;

// Atomic counter is used to track the next availibale slot
groupshared Atomic<uint32_t> visibleMeshletCount;

// Object space frustum planes, shared by every thread in the group
groupshared float4 frustumPlanes[6];

void ExtractFrustumPlanes()
{
    // Object space clip matrix of the camera
    const float4x4 cullMatrix = mul(sceneCamera.viewProjection, model);

    frustumPlanes[0] = cullMatrix[3] + cullMatrix[0]; // Left
    frustumPlanes[1] = cullMatrix[3] - cullMatrix[0]; // Right
    frustumPlanes[2] = cullMatrix[3] + cullMatrix[1]; // Bottom
    frustumPlanes[3] = cullMatrix[3] - cullMatrix[1]; // Top
    frustumPlanes[4] = cullMatrix[2];                 // Near
    frustumPlanes[5] = cullMatrix[3] - cullMatrix[2]; // Far

    for (int i = 0; i < 6; i++)
    {
        frustumPlanes[i] /= length(frustumPlanes[i].xyz);
    }
}

bool IsInFrustum(float3 center, float radius)
{
    for (int i = 0; i < 6; i++)
    {
        if (dot(frustumPlanes[i].xyz, center) + frustumPlanes[i].w < -radius)
        {
            return false;
        }
    }
    return true;
}

// Backface cone culling in object space rejects meshlets whose triangles all face away from the camera
bool IsConeVisible(Meshlet meshlet)
{
    float3 cameraToCenter = meshlet.meshletBoundingSphere.xyz - cullCameraPosition;
    float cameraToCenterDist = length(cameraToCenter);

    return dot(cameraToCenter, meshlet.coneAxis) < meshlet.coneCutoff * cameraToCenterDist + meshlet.meshletBoundingSphere.w;
}

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_X, 1, 1)]
void main(uint3 gid: SV_GroupID, uint3 gtid: SV_GroupThreadID) {
    StructuredBuffer<Meshlet> meshlets = DescriptorHandle<StructuredBuffer<Meshlet>>(meshletBufferView);

    // meshletOffset and meshletCount describe the slice of the mesh this dispatch covers
    uint32_t groupMeshletStart = gid.x * TASK_SHADER_DISPATCH_X;
    uint32_t groupMeshletOffset = meshletOffset + groupMeshletStart;

    // Initialize shared memory for the first thread
    if (gtid.x == 0) {
        visibleMeshletCount.store(0);
        sharedPayload.groupMeshletOffset = groupMeshletOffset;
        ExtractFrustumPlanes();
    }
    // Sync the 0 initialization with the other memory
    GroupMemoryBarrierWithGroupSync();

    // Each thread tests one meshlet, the last group can be partial
    if (groupMeshletStart + gtid.x < meshletCount) {
        Meshlet meshlet = meshlets[groupMeshletOffset + gtid.x];
        float4 sphere = meshlet.meshletBoundingSphere;

        if (IsInFrustum(sphere.xyz, sphere.w) && IsConeVisible(meshlet)) {
            uint index = visibleMeshletCount.add(1);
            sharedPayload.meshletIndices[index] = gtid.x;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(visibleMeshletCount.load(), 1, 1, sharedPayload);
}
