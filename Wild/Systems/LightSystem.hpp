#pragma once

#include "Systems/SystemManager.hpp"

#include <cstdint>
#include <memory>

namespace Wild
{
    class Buffer;

    class LightSystem : public System
    {
      public:
        static constexpr uint32_t MAX_POINT_LIGHTS = 15;

        LightSystem();

        void Update();

        std::shared_ptr<Buffer> GetPointLightBuffer() { return m_pointLightBuffer; }
        uint32_t GetPointLightCount() const noexcept { return m_pointLightCount; }

      private:
        std::shared_ptr<Buffer> m_pointLightBuffer{};
        uint32_t m_pointLightCount{};
    };
} // namespace Wild
