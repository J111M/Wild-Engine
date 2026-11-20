#pragma once

#include "Systems/SystemManager.hpp"

#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

namespace Wild
{
    class GPUBuffer;

    // Camera centred probe cascades, the spacing doubles per cascade.
    // Probes are stored toroidally so they keep their history while a cascade scrolls with the camera.
    class ProbeSystem : public System
    {
      public:
        struct ProbeRayData
        {
            glm::vec3 radiance{};
            float hitDistance{};
        };

        struct ProbeIrradiance
        {
            glm::vec4 irradiance{};
        };

        struct Cascade
        {
            glm::ivec3 originCell{};    // Volume min corner in cells of the history atlas
            glm::ivec3 newOriginCell{}; // Volume min corner written this frame
            float spacing{1.0f};
            int32_t traceSlot{-1}; // Ray buffer slot, -1 when not traced this frame
        };

        static constexpr uint32_t MAX_RAYS_PER_PROBE = 256;
        static constexpr uint32_t MAX_CASCADES = 4;

        // Cascade without history
        static constexpr int32_t INVALID_ORIGIN_CELL = -(1 << 20);

        // Counts and cascade count size the atlases, they can't change after construction
        ProbeSystem(const glm::ivec3& countsPerCascade, uint32_t cascadeCount, float baseSpacing, uint32_t maxTracedPerFrame);

        void Update(const glm::vec3& cameraPosition);

        void Invalidate();

        void SetBaseSpacing(float spacing);
        float GetBaseSpacing() const noexcept { return m_baseSpacing; }

        void SetCascadesTracedPerFrame(uint32_t count);
        uint32_t GetCascadesTracedPerFrame() const noexcept { return m_tracedPerFrame; }
        uint32_t GetMaxCascadesTracedPerFrame() const noexcept { return m_maxTracedPerFrame; }

        const glm::ivec3& GetCounts() const noexcept { return m_counts; }
        uint32_t GetCascadeCount() const noexcept { return m_cascadeCount; }
        uint32_t GetProbesPerCascade() const noexcept { return static_cast<uint32_t>(m_counts.x * m_counts.y * m_counts.z); }
        uint32_t GetTotalProbeCount() const noexcept { return GetProbesPerCascade() * m_cascadeCount; }

        const Cascade& GetCascade(uint32_t index) const { return m_cascades[index]; }

        // Index is the ray buffer slot
        const std::vector<uint32_t>& GetTracedCascades() const noexcept { return m_tracedCascades; }

        std::shared_ptr<GPUBuffer> GetProbeRayDataBuffer() { return m_probeRayData; }
        std::shared_ptr<GPUBuffer> GetProbeIrradianceBuffer() { return m_probeIrradiance; }

      private:
        void AllocateProbeGIBuffers();

        std::shared_ptr<GPUBuffer> m_probeRayData{};
        std::shared_ptr<GPUBuffer> m_probeIrradiance{};

        glm::ivec3 m_counts;
        uint32_t m_cascadeCount;
        float m_baseSpacing;

        uint32_t m_maxTracedPerFrame;
        uint32_t m_tracedPerFrame;

        uint32_t m_nextRoundRobinCascade = 1;

        std::array<Cascade, MAX_CASCADES> m_cascades{};
        std::vector<uint32_t> m_tracedCascades;
    };
} // namespace Wild
