#pragma once

namespace Wild
{
    class FrustumCullingSystem
    {
      public:
        FrustumCullingSystem();
        ~FrustumCullingSystem();

        const std::vector<Entity>& CullMeshComponents(const Camera& camera);

      private:
        std::vector<Entity> m_visible;
    };
} // namespace Wild
