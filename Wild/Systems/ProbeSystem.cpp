#include "Systems/ProbeSystem.hpp"

#include "Renderer/Resources/Buffer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace Wild
{
    ProbeSystem::ProbeSystem(const glm::ivec3& countsPerCascade, uint32_t cascadeCount, float baseSpacing, uint32_t maxTracedPerFrame)
        : m_counts(countsPerCascade), m_cascadeCount(std::clamp(cascadeCount, 1u, MAX_CASCADES)), m_baseSpacing(baseSpacing)
    {
        assert(countsPerCascade.x > 0 && countsPerCascade.y > 0 && countsPerCascade.z > 0);
        assert(baseSpacing > 0.0f);

        m_maxTracedPerFrame = std::clamp(maxTracedPerFrame, 1u, m_cascadeCount);
        m_tracedPerFrame = m_maxTracedPerFrame;

        Invalidate();
        AllocateProbeGIBuffers();
    }

    void ProbeSystem::Invalidate()
    {
        for (uint32_t i = 0; i < MAX_CASCADES; ++i)
        {
            Cascade& cascade = m_cascades[i];
            cascade.spacing = m_baseSpacing * static_cast<float>(1u << i);
            cascade.originCell = glm::ivec3(INVALID_ORIGIN_CELL);
            cascade.newOriginCell = glm::ivec3(INVALID_ORIGIN_CELL);
            cascade.traceSlot = -1;
        }

        m_tracedCascades.clear();
    }

    void ProbeSystem::SetBaseSpacing(float spacing)
    {
        if (spacing <= 0.0f || spacing == m_baseSpacing) return;

        m_baseSpacing = spacing;
        Invalidate();
    }

    void ProbeSystem::SetCascadesTracedPerFrame(uint32_t count) { m_tracedPerFrame = std::clamp(count, 1u, m_maxTracedPerFrame); }

    void ProbeSystem::Update(const glm::vec3& cameraPosition)
    {
        // Last frame's writes are this frame's history
        for (uint32_t i = 0; i < m_cascadeCount; ++i)
        {
            Cascade& cascade = m_cascades[i];
            cascade.originCell = cascade.newOriginCell;
            cascade.traceSlot = -1;
        }

        // Cascade 0 is traced every frame, the others round robin
        m_tracedCascades.clear();
        m_tracedCascades.push_back(0);

        const uint32_t farCascades = m_cascadeCount - 1;
        const uint32_t farSlots = std::min(m_tracedPerFrame - 1, farCascades);
        for (uint32_t i = 0; i < farSlots; ++i)
        {
            m_tracedCascades.push_back(m_nextRoundRobinCascade);
            m_nextRoundRobinCascade = (m_nextRoundRobinCascade % farCascades) + 1;
        }

        // Snapping to whole cells keeps the probes at fixed world positions
        const glm::ivec3 halfCounts = m_counts / 2;
        for (uint32_t slot = 0; slot < m_tracedCascades.size(); ++slot)
        {
            Cascade& cascade = m_cascades[m_tracedCascades[slot]];
            const glm::ivec3 cameraCell = glm::ivec3(glm::floor(cameraPosition / cascade.spacing));

            cascade.newOriginCell = cameraCell - halfCounts;
            cascade.traceSlot = static_cast<int32_t>(slot);
        }
    }

    void ProbeSystem::AllocateProbeGIBuffers()
    {
        // Laid out as [slot][probe][ray]
        {
            const uint64_t elementCount = static_cast<uint64_t>(GetProbesPerCascade()) * MAX_RAYS_PER_PROBE * m_maxTracedPerFrame;

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
            const uint32_t probeCount = GetTotalProbeCount();

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
} // namespace Wild
