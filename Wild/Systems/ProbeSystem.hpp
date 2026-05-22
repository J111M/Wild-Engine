#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Wild
{
    class ProbeSystem
    {
      public:
        struct Probe
        {
            glm::vec4 position;
        };

        ProbeSystem(const glm::vec3& origin, const glm::vec3& spacing, const glm::ivec3& counts);

        const glm::vec3& GetOrigin() const noexcept { return m_origin; }
        const glm::vec3& GetSpacing() const noexcept { return m_spacing; }
        const glm::ivec3& GetCounts() const noexcept { return m_counts; }

        uint32_t GetProbeCount() const noexcept { return static_cast<uint32_t>(m_counts.x * m_counts.y * m_counts.z); }

        const std::vector<Probe>& GetProbes() const noexcept { return m_probes; }
        std::shared_ptr<Buffer> GetProbeBuffer() { return m_probeStructure; }

        size_t GetByteSize() const noexcept { return m_probes.size() * sizeof(Probe); }

        void SetOrigin(const glm::vec3& origin);

        // Allocate probes for 1 time for now until a cascade system is implemented
        void AllocateProbes();
      private:
        void Generate();

        //bool m_markDirty = false;

        std::shared_ptr<Buffer> m_probeStructure{};

        glm::vec3 m_origin;
        glm::vec3 m_spacing;
        glm::ivec3 m_counts;
        std::vector<Probe> m_probes;
    };
}