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

    bool OceanChunkSystem::IsChunkInsideFrustum(const BoundingFrustum& frustum, const OceanChunk& chunk) { 
        glm::vec3 min = {chunk.coord.x * m_chunkSize, 0.0f, chunk.coord.y * m_chunkSize};
        glm::vec3 max = {min.x + m_chunkSize, 32.0f, min.z + m_chunkSize};

        for (const auto& plane : frustum.planes) {
            glm::vec3 positiveVertex = {(plane.x >= 0) ? max.x : min.x,
                                        (plane.y >= 0) ? max.y : min.y,
                                        (plane.z >= 0) ? max.z : min.z};

            // Compute the distance of the positive vertex to the plane
            float distance = plane.x * positiveVertex.x + plane.y * positiveVertex.y + plane.z * positiveVertex.z + plane.w;


            if (distance < 0) { return false; }
        }

        return true; 
    }

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

    void OceanChunkSystem::Update(const glm::vec3& cameraPos, const BoundingFrustum& frustum, bool updateFrustum)
    {
        glm::vec2 centerCoord = glm::vec2(std::floor(cameraPos.x / m_chunkSize), std::floor(cameraPos.z / m_chunkSize));

        glm::vec2 cameraXZ = glm::vec2(cameraPos.x, cameraPos.z);
        for (auto& chunk : m_chunks)
        {
            // Check what level of detail the chunk needs depending on the distance from the center
            glm::vec2 chunkCenter = chunk.coord * m_chunkSize;
            float dist = glm::distance(cameraXZ, chunkCenter);

            uint32_t newLod;
            if (dist < m_chunkSize * 2.0f)
                newLod = 0;
            else if (dist < m_chunkSize * 4.0f)
                newLod = 1;
            else
                newLod = 2;

            // Check if the chunk is inside the frustum
            if (updateFrustum) { chunk.isVisible = IsChunkInsideFrustum(frustum, chunk); } 

            chunk.lod = newLod;
        }

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
