#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_set>
#include <vector>

namespace Wild
{
    struct OceanChunk
    {
        glm::vec2 coord{};
        uint32_t lod{};
        Entity entity{};
        bool isVisible = true;
    };

    class OceanChunkSystem : public NonCopyable
    {
      public:
        OceanChunkSystem(float chunkSize, int viewRange, float yLevel = -9.0f);
        ~OceanChunkSystem();

        void Update(const glm::vec3& cameraPos, const BoundingFrustum& frustum, bool updateFrustum);

        void ClearChunks();

        const std::vector<OceanChunk> GetOceanChunks() const { return m_chunks; }

      private:
        bool IsChunkInsideFrustum(const BoundingFrustum& frustum, const OceanChunk& chunk);

        void CreateChunk(const glm::vec2& coord);

        float m_chunkSize;
        int m_viewRange;
        float m_yLevel;
        glm::vec2 m_lastCenteredCoord = glm::vec2(FLT_MAX);
        std::vector<OceanChunk> m_chunks;

        Entity m_chunkSystemEntity{};
    };
} // namespace Wild
