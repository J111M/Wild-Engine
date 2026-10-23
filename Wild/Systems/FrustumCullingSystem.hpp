#pragma once

#include "Core/ECS.hpp"

#include "Renderer/Resources/BoundingFrustum.hpp"

#include "Systems/SystemManager.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Wild
{
    class FrustumCullingSystem : public System
    {
      public:
        FrustumCullingSystem() = default;
        ~FrustumCullingSystem() override = default;

        // Returns every mesh entity whose world space bounds intersect the frustum of the view projection matrix.
        // Entities keep the order of the Transform/MeshComponent view, the result is overwritten by the next call.
        const std::vector<Entity>& CullMeshComponents(const glm::mat4& viewProjection);

        [[nodiscard]] static BoundingFrustum ExtractFrustum(const glm::mat4& matrix);

      private:
        std::vector<Entity> m_visible;
    };
} // namespace Wild
