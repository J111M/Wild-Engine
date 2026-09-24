#include "Helper/MeshCommon.slang"

const static uint TASK_SHADER_DISPATCH_X = 64;

cbuffer RootConsant : register(b0)
{
    uint meshletCount;
};

groupshared Atomic<uint32_t> visibleMeshletCount;

[shader("task")]
[numthreads(TASK_SHADER_DISPATCH_X, 1, 1)]
void main(uint3 gid: SV_GroupID, uint3 gtid: SV_GroupThreadID) {
    uint32_t groupMeshletOffset = (gid.x * TASK_SHADER_DISPATCH_X);
    uint32_t totalMeshlets = meshletCount;

    // Initialize shared memory for the first thread
    if (gtid.x == 0) {
        visibleMeshletCount.store(0);
        sharedPayload.groupMeshletOffset = groupMeshletOffset;
    }

    // Sync the 0 initialization with the other memory
    GroupMemoryBarrierWithGroupSync();

    // Handling partial groups
    uint8_t threadMeshletOffset = (uint8_t)gtid.x;

    uint32_t groupMeshletCount = min(TASK_SHADER_DISPATCH_X, totalMeshlets - groupMeshletOffset);

    if (threadMeshletOffset < groupMeshletCount) {
        uint32_t meshletIndex = groupMeshletOffset + threadMeshletOffset;
    }

    GroupMemoryBarrierWithGroupSync();

    // First thread dispatches the mesh shaders for all visible meshlets
    if (gtid.x == 0) {
        DispatchMesh(visibleMeshletCount.load(), 1, 1, sharedPayload);

        // Each thread test one meshlet
        uint visible = 0;
        if (visible > 0) {
            uint index = visibleMeshletCount.add(1);

            // local offset of 64 fits in a uint8_t
            sharedPayload.meshletIndices[index] = gtid.x;
        }
    }
}