#include "Systems/ProbeSystem.hpp"

#include <cstdint>
#include <cassert>

namespace Wild
{
    ProbeSystem::ProbeSystem(const glm::vec3& origin, const glm::vec3& spacing, const glm::ivec3& counts)
        : m_origin(origin), m_spacing(spacing), m_counts(counts)
    {
        assert(counts.x > 0 && counts.y > 0 && counts.z > 0);
        assert(spacing.x > 0.0f && spacing.y > 0.0f && spacing.z > 0.0f);
        Generate();
    }

    void ProbeSystem::SetOrigin(const glm::vec3& origin)
    {
        m_origin = origin;
        Generate();
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
                    p.position = m_origin + glm::vec3(x, y, z) * m_spacing;
                    m_probes.push_back(p);
                }
    }
} // namespace Wild