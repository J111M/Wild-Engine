#include "Systems/FrustumCullingSystem.hpp"

namespace Wild
{
    FrustumCullingSystem::FrustumCullingSystem() {}

    const std::vector<Entity>& FrustumCullingSystem::CullMeshComponents(const Camera& camera)
    {
        // Reuse vector container to prevent unnecessary heap allocs
        m_visible.clear();

        auto ecs = engine.GetECS();
        const auto& frustumBounds = camera.GetFrustum();

        entt::view<entt::get_t<Transform, MeshComponent>> meshes = ecs->GetRegistry().view<Transform, MeshComponent>();

        // Check if the current capcity is bigger than the estimated number of entities
        if (m_visible.capacity() < meshes.size_hint()) m_visible.reserve(meshes.size_hint());

        for (auto&& [entity, trans, meshComponent] : meshes.each())
        {
            if (!meshComponent.mesh) continue;
            auto& mesh = *meshComponent.mesh;
        }

        return m_visible;
    }
} // namespace Wild
