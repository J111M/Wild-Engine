#pragma once

#include "Renderer/PerFrame.hpp"
#include "Systems/SystemManager.hpp"

#include <cstdint>
#include <memory>

namespace Wild
{
    class GPUBuffer;

    class LightSystem : public System
    {
      public:
        static constexpr uint32_t MAX_POINT_LIGHTS = 15;

        LightSystem();

        void Update();

        // The point light buffer of the frame that is being recorded
        std::shared_ptr<GPUBuffer> GetPointLightBuffer() { return m_pointLightBuffers.Get(); }
        uint32_t GetPointLightCount() const noexcept { return m_pointLightCount; }

      private:
        // One copy per frame in flight
        PerFrame<std::shared_ptr<GPUBuffer>> m_pointLightBuffers;
        uint32_t m_pointLightCount{};
    };
} // namespace Wild
