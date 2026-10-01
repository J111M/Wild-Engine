#pragma once

// Mirrors SceneCameraData in Wild/Renderer/Resources/SceneData.hpp, keep both in sync.
// Bind it with: ConstantBuffer<SceneCameraData> sceneCamera : register(bX);

typedef column_major float4x4 SceneMatrix;

struct SceneCameraData
{
    SceneMatrix view;
    SceneMatrix projection;
    SceneMatrix viewProjection;
    SceneMatrix inverseView;
    SceneMatrix inverseProjection;
    SceneMatrix inverseViewProjection;

    float4 position;

    // x = near, y = far, z = vertical fov in radians, w = aspect ratio
    float4 nearFarFovAspect;

    // Normalized world space planes in the order left, right, bottom, top, near, far
    float4 frustumPlanes[6];
};
