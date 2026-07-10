#include "Systems/ProbeSystem.hpp"

#include <cassert>
#include <cstdint>

namespace Wild
{
    ProbeSystem::ProbeSystem(const glm::vec3& origin, const glm::vec3& spacing, const glm::ivec3& counts)
        : m_origin(origin), m_spacing(spacing), m_counts(counts)
    {
        assert(counts.x > 0 && counts.y > 0 && counts.z > 0);
        assert(spacing.x > 0.0f && spacing.y > 0.0f && spacing.z > 0.0f);
        Generate();

        AllocateProbes();
        AllocateProbeGIBuffers();
    }

    void ProbeSystem::SetOrigin(const glm::vec3& origin)
    {
        m_origin = origin;
        Generate();
    }

    void ProbeSystem::AllocateProbes()
    {
        m_probeStructure.reset();

        // Create structured probe buffer
        BufferDesc desc{};
        desc.bufferSize = sizeof(Probe);
        desc.numOfElements = m_probes.size();
        m_probeStructure = std::make_shared<Buffer>(desc, structured);
        m_probeStructure->UploadToGPU(m_probes.data(), m_probes.size() * sizeof(Probe));
    }

    void ProbeSystem::AllocateProbeGIBuffers()
    {
        const uint32_t probeCount = GetProbeCount();

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(ProbeRayData);
            desc.numOfElements = probeCount * MAX_RAYS_PER_PROBE;
            m_probeRayData = std::make_shared<Buffer>(desc, BufferType::uav);

            std::vector<ProbeRayData> zeroed(desc.numOfElements);
            m_probeRayData->UploadToGPU(zeroed.data(), zeroed.size() * sizeof(ProbeRayData));
        }

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(ProbeIrradiance);
            desc.numOfElements = probeCount;
            m_probeIrradiance = std::make_shared<Buffer>(desc, BufferType::uav);

            std::vector<ProbeIrradiance> zeroed(desc.numOfElements);
            m_probeIrradiance->UploadToGPU(zeroed.data(), zeroed.size() * sizeof(ProbeIrradiance));
        }
    }

    void ProbeSystem::Generate()
    {
        m_probes.clear();
        m_probes.reserve(GetProbeCount());
        for (int z = 0; z < m_counts.z; ++z)
            for (int y = 0; y < m_counts.y; ++y)
                for (int x = 0; x < m_counts.x; ++x)
                {
                    Probe p{};
                    p.position = glm::vec4(m_origin, 1.0f) + glm::vec4(x, y, z, 1.0f) * glm::vec4(m_spacing, 1.0f);
                    m_probes.push_back(p);
                }
    }
} // namespace Wild
