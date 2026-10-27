#include "Systems/FrustumCullingSystem.hpp"

#include "Renderer/Resources/Mesh.hpp"

#include <glm/gtc/matrix_access.hpp>
#include <algorithm>
#include <execution>
#include <numeric>
#include <thread>

namespace Wild
{
    const std::vector<Entity>& FrustumCullingSystem::CullMeshComponents(const glm::mat4& viewProjection)
    {
        // Reuse vector container to prevent unnecessary heap allocs
        m_visible.clear();

        auto ecs = engine.GetECS();
        const auto& frustumBounds = FrustumCullingSystem::ExtractFrustum(viewProjection);

        entt::view<entt::get_t<Transform, MeshComponent>> meshes = ecs->GetRegistry().view<Transform, MeshComponent>();

        // Check if the current capcity is bigger than the estimated number of entities
        if (m_visible.capacity() < meshes.size_hint()) m_visible.reserve(meshes.size_hint());

        for (auto&& [entity, trans, meshComponent] : meshes.each())
        {
            if (!meshComponent.mesh) continue;
            // Coarse test only, the mesh shader pass culls the individual meshlets precisely
            const BoundingSphere sphere = meshComponent.mesh->GetBoundingSphere().Transformed(trans.GetWorldMatrix());
            if (frustumBounds.Intersect(sphere)) { m_visible.emplace_back(entity); }
        }

        return m_visible;
    }

    BoundingFrustum FrustumCullingSystem::ExtractFrustum(const glm::mat4& matrix)
    {
        const glm::vec4 r0 = glm::row(matrix, 0);
        const glm::vec4 r1 = glm::row(matrix, 1);
        const glm::vec4 r2 = glm::row(matrix, 2);
        const glm::vec4 r3 = glm::row(matrix, 3);

        BoundingFrustum frustum{};
        frustum.planes[Left] = r3 + r0;
        frustum.planes[Right] = r3 - r0;
        frustum.planes[Bottom] = r3 + r1;
        frustum.planes[Top] = r3 - r1;
        // Depth runs from 0 to 1 (GLM_FORCE_DEPTH_ZERO_TO_ONE) so the near plane is z >= 0 instead of z >= -w
        frustum.planes[Near] = r2;
        frustum.planes[Far] = r3 - r2;

        // Normalize frustum planes
        for (auto& plane : frustum.planes)
        {
            const float len = glm::length(glm::vec3(plane));
            plane /= len;
        }

        return frustum;
    }
} // namespace Wild
