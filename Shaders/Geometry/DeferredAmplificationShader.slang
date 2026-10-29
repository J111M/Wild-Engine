#include "Helper/Common.slang"
#include "Helper/MeshCommon.slang"

// Late pass define macro
#ifndef LATE_PASS
#define LATE_PASS 0
#endif

#if LATE_PASS
// Point sampling so the HZB returns the stored farthest depth
SamplerState hzbPointSampler : register(s1);

// Tests an object space bounding sphere against the HZB built from the early pass depth
bool IsOcclusionCulled(float3 center, float radius)
{
    // View space sphere, the camera looks down -z so z is flipped to get a positive distance in front of the camera
    const float3 viewCenter = mul(sceneCamera.view, mul(model, float4(center, 1.0f))).xyz;
    const float3 c = float3(viewCenter.xy, -viewCenter.z);
    const float scale = max(max(length(model._m00_m10_m20), length(model._m01_m11_m21)), length(model._m02_m12_m22));
    const float r = radius * scale;

    // Spheres crossing the near plane cannot be projected, so they are kept visible
    const float zNear = sceneCamera.nearFarFovAspect.x;
    if (c.z - r < zNear) return false;

    // Screen space bounds of the sphere, from 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere (Mara and McGuire 2013)
    const float P00 = sceneCamera.projection[0][0];
    const float P11 = sceneCamera.projection[1][1];

    const float3 cr = c * r;
    const float czr2 = c.z * c.z - r * r;

    const float vx = sqrt(c.x * c.x + czr2);
    const float minX = (vx * c.x - cr.z) / (vx * c.z + cr.x);
    const float maxX = (vx * c.x + cr.z) / (vx * c.z - cr.x);

    const float vy = sqrt(c.y * c.y + czr2);
    const float minY = (vy * c.y - cr.z) / (vy * c.z + cr.y);
    const float maxY = (vy * c.y + cr.z) / (vy * c.z - cr.y);

    // NDC to uv as (minU, minV, maxU, maxV), v points down so the y bounds swap
    float4 aabb = float4(minX * P00, maxY * P11, maxX * P00, minY * P11) * float4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f;
    aabb = saturate(aabb);

    // Mip where the rectangle is at most one texel wide, so its corners land in a 2x2 block
    const float2 sizePixels = (aabb.zw - aabb.xy) * float2(hzbSize);
    const float level = clamp(ceil(log2(max(max(sizePixels.x, sizePixels.y), 1.0f))), 0.0f, float(hzbMipCount - 1));

    Texture2D<float> hzb = DescriptorHandle<Texture2D<float>>(hzbView);
    const float s0 = hzb.SampleLevel(hzbPointSampler, aabb.xy, level);
    const float s1 = hzb.SampleLevel(hzbPointSampler, aabb.zy, level);
    const float s2 = hzb.SampleLevel(hzbPointSampler, aabb.xw, level);
    const float s3 = hzb.SampleLevel(hzbPointSampler, aabb.zw, level);

    // Farthest occluder depth in the block
    const float maxZ = max(max(s0, s1), max(s2, s3));

    // Depth of the sphere point closest to the camera
    const float4 nearestClip = mul(sceneCamera.projection, float4(0.0f, 0.0f, -(c.z - r), 1.0f));
    const float sphereDepth = nearestClip.z / nearestClip.w;

    // Depth runs from 0 at the near plane to 1 at the far plane, so a larger depth is behind every occluder
    return sphereDepth > maxZ;
}
#endif

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
        // An infinite projection has no far plane never cull it
        const float len = length(frustumPlanes[i].xyz);
        frustumPlanes[i] = len < 1e-6f ? float4(0.0f, 0.0f, 0.0f, 1.0f) : frustumPlanes[i] / len;
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

        // Draws only what was visible last frame
        uint32_t visibilityIndex = instanceMeshletOffset + groupMeshletOffset + gtid.x;
        bool visible = true;
        if (meshletVisibilityView != INVALID_HEAP_INDEX) {
            RWStructuredBuffer<uint32_t> meshletVisibility = DescriptorHandle<RWStructuredBuffer<uint32_t>>(meshletVisibilityView);
            visible = meshletVisibility[visibilityIndex] != 0;
        }

        const bool inView = IsInFrustum(sphere.xyz, sphere.w) && IsConeVisible(meshlet);

#if LATE_PASS
        // Late pass, draws what became visible this frame and stores the visibility for the next frame
        const bool visibleNow = inView && !IsOcclusionCulled(sphere.xyz, sphere.w);

        // Meshlets the early pass already drew are skipped
        const bool draw = visibleNow && !visible;

        if (meshletVisibilityView != INVALID_HEAP_INDEX) {
            RWStructuredBuffer<uint32_t> meshletVisibility = DescriptorHandle<RWStructuredBuffer<uint32_t>>(meshletVisibilityView);
            meshletVisibility[visibilityIndex] = visibleNow ? 1 : 0;
        }
#else
        const bool draw = visible && inView;
#endif

        if (draw) {
            uint index = visibleMeshletCount.add(1);
            sharedPayload.meshletIndices[index] = gtid.x;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    DispatchMesh(visibleMeshletCount.load(), 1, 1, sharedPayload);
}
