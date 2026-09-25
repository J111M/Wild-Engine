#include "Helper/MeshCommon.slang"

groupshared MeshletPayload sharedPayload;

// Atomic counter is used to track the next availibale slot
groupshared Atomic<uint32_t> visibleMeshletCount;

// Object space frustum planes, shared by every thread in the group
groupshared float4 frustumPlanes[6];

// TODO use precomputed frustum planes
void ExtractFrustumPlanes()
{
    frustumPlanes[0] = model[3] + model[0]; // Left
    frustumPlanes[1] = model[3] - model[0]; // Right
    frustumPlanes[2] = model[3] + model[1]; // Bottom
    frustumPlanes[3] = model[3] - model[1]; // Top
    frustumPlanes[4] = model[2];            // Near
    frustumPlanes[5] = model[3] - model[2]; // Far

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
        float4 sphere = meshlets[groupMeshletOffset + gtid.x].meshletBoundingSphere;

        if (IsInFrustum(sphere.xyz, sphere.w)) {
            uint index = visibleMeshletCount.add(1);
            sharedPayload.meshletIndices[index] = gtid.x;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Thread 0 dispatches mesh shaders for visible meshlets only
    if (gtid.x == 0) {
        DispatchMesh(visibleMeshletCount.load(), 1, 1, sharedPayload);
    }
}
