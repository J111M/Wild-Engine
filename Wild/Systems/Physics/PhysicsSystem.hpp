#pragma once

#include "Core/ECS.hpp"
#include "Systems/Physics/PhysicsTypes.hpp"
#include "Systems/SystemManager.hpp"

#include <memory>

namespace Wild
{
    class Transform;

    class PhysicsSystem final : public System
    {
      public:
        PhysicsSystem();
        ~PhysicsSystem() override;

        void Update(float deltaTime);

        // Removes and destroys the Jolt body owned by this RigidBody, if any.
        // Called before an entity carrying a RigidBody is destroyed.
        void DestroyBody(RigidBody& rigidBody);

      private:
        void RebuildBody(Entity entity, RigidBody& rigidBody, Transform& transform);
        void DrawDebugShapes();

        struct Impl;
        std::unique_ptr<Impl> m_impl;

        float m_accumulator = 0.0f;
    };
} // namespace Wild
