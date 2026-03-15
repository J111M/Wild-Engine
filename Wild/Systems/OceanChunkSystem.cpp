#include "Systems/OceanChunkSystem.hpp"

namespace Wild
{
    OceanChunkSystem::OceanChunkSystem(float chunkSize, int viewRange, float yLevel)
        : m_chunkSize(chunkSize), m_viewRange(viewRange), m_yLevel(yLevel)
    {
        m_chunkSystemEntity = engine.GetECS()->CreateEntity();
        auto& transform = engine.GetECS()->AddComponent<Transform>(m_chunkSystemEntity, glm::vec3(0), m_chunkSystemEntity);
        transform.Name = "Ocean chunk system";
    }

    OceanChunkSystem::~OceanChunkSystem() { ClearChunks(); }

    void OceanChunkSystem::CreateChunk(const glm::vec2& coord)
    {
        OceanChunk chunk;
        chunk.coord = coord;
        chunk.entity = engine.GetECS()->CreateEntity();

        auto& transform = engine.GetECS()->AddComponent<Transform>(chunk.entity, glm::vec3(0), chunk.entity);
        transform.SetPosition(glm::vec3(coord.x * m_chunkSize, m_yLevel, coord.y * m_chunkSize));

        std::string chunkName = "Chunk: " + std::to_string(coord.x) + " " + std::to_string(coord.y);
        transform.Name = chunkName;

        transform.SetParent(m_chunkSystemEntity);

        m_chunks.push_back(chunk);
    }

    void OceanChunkSystem::Update(const glm::vec3& cameraPos)
    {
        glm::vec2 centerCoord = glm::vec2(std::floor(cameraPos.x / m_chunkSize), std::floor(cameraPos.z / m_chunkSize));

        if (centerCoord == m_lastCenteredCoord && !m_chunks.empty()) return;

        m_lastCenteredCoord = centerCoord;

        // Calculate new chunks
        std::unordered_set<glm::vec2> desired;
        for (int x = -m_viewRange; x <= m_viewRange; ++x)
        {
            for (int z = -m_viewRange; z <= m_viewRange; ++z)
            {
                desired.insert(centerCoord + glm::vec2(x, z));
            }
        }

        // Remove chunks that are out of range
        for (auto it = m_chunks.begin(); it != m_chunks.end();)
        {
            if (desired.find(it->coord) == desired.end())
            {
                engine.GetECS()->DestroyEntity(it->entity);
                it = m_chunks.erase(it);
            }
            else
            {
                desired.erase(it->coord); // Chunk already exsists
                ++it;
            }
        }

        // Create new chunks for remaining desired coords
        for (const auto& coord : desired)
        {
            CreateChunk(coord);
        }

        glm::vec2 cameraXZ = glm::vec2(cameraPos.x, cameraPos.z);
        for (auto& chunk : m_chunks)
        {
            glm::vec2 chunkCenter = chunk.coord * m_chunkSize;
            float dist = glm::distance(cameraXZ, chunkCenter);

            uint32_t newLod;
            if (dist < m_chunkSize * 2.0f)
                newLod = 0;
            else if (dist < m_chunkSize * 4.0f)
                newLod = 1;
            else
                newLod = 2;

            chunk.lod = newLod;
        }
    }

    void OceanChunkSystem::ClearChunks()
    {
        for (auto& chunk : m_chunks)
        {
            engine.GetECS()->DestroyEntity(chunk.entity);
        }
        m_chunks.clear();
    }

} // namespace Wild
