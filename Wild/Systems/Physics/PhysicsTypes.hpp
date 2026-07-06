#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace Wild
{
    enum class MotionType
    {
        Static,
        Kinematic,
        Dynamic
    };

    enum class ColliderType
    {
        Box,
        Sphere,
        Capsule,
        ConvexHull,
        Mesh
    };

    struct ColliderShape
    {
        ColliderType type = ColliderType::Box;

        glm::vec3 offset{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

        glm::vec3 boxHalfExtents{0.5f};
        float sphereRadius = 0.5f;
        float capsuleRadius = 0.5f;
        float capsuleHalfHeight = 0.5f;

        float friction = 0.5f;
        float restitution = 0.0f;
    };

    inline constexpr uint32_t kInvalidBodyId = 0xFFFFFFFFu;

    struct RigidBody
    {
        MotionType motionType = MotionType::Static;
        bool useGravity = true;
        bool isTrigger = false;

        bool autoMass = true;
        float mass = 1.0f;

        float linearDamping = 0.05f;
        float angularDamping = 0.05f;

        std::vector<ColliderShape> shapes;

        uint32_t bodyId = kInvalidBodyId;
        bool bodyDirty = true;
        glm::vec3 lastSyncedScale{-1.0f};

        glm::vec3 lastSyncedPosition{0.0f};
        glm::quat lastSyncedRotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
} // namespace Wild
