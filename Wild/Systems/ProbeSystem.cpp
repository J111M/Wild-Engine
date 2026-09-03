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

        if (m_probeStructure) m_probeStructure->UploadToGPU(m_probes.data(), m_probes.size() * sizeof(Probe));
    }

    void ProbeSystem::AllocateProbes()
    {
        m_probeStructure.reset();

        // Create structured probe buffer
        BufferDesc desc{};
        desc.size = static_cast<uint64_t>(sizeof(Probe)) * m_probes.size();
        desc.stride = sizeof(Probe);
        desc.usage = BufferUsage::ShaderRead;
        desc.access = MemoryAccess::GpuOnly;
        m_probeStructure = std::make_shared<GPUBuffer>(desc);
        m_probeStructure->UploadToGPU(m_probes.data(), m_probes.size() * sizeof(Probe));
    }

    void ProbeSystem::AllocateProbeGIBuffers()
    {
        const uint32_t probeCount = GetProbeCount();

        {
            const uint32_t elementCount = probeCount * MAX_RAYS_PER_PROBE;

            BufferDesc desc{};
            desc.size = static_cast<uint64_t>(sizeof(ProbeRayData)) * elementCount;
            desc.stride = sizeof(ProbeRayData);
            desc.usage = BufferUsage::ShaderWrite;
            desc.access = MemoryAccess::GpuOnly;
            m_probeRayData = std::make_shared<GPUBuffer>(desc);

            std::vector<ProbeRayData> zeroed(elementCount);
            m_probeRayData->UploadToGPU(zeroed.data(), zeroed.size() * sizeof(ProbeRayData));
        }

        {
            BufferDesc desc{};
            desc.size = static_cast<uint64_t>(sizeof(ProbeIrradiance)) * probeCount;
            desc.stride = sizeof(ProbeIrradiance);
            desc.usage = BufferUsage::ShaderWrite;
            desc.access = MemoryAccess::GpuOnly;
            m_probeIrradiance = std::make_shared<GPUBuffer>(desc);

            std::vector<ProbeIrradiance> zeroed(probeCount);
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
