#include "Systems/LightSystem.hpp"

#include "Renderer/Resources/LightTypes.hpp"

#include <vector>

namespace Wild
{
    LightSystem::LightSystem()
    {
        BufferDesc desc{};
        desc.bufferSize = sizeof(PointLight) * MAX_POINT_LIGHTS;
        m_pointLightBuffer = std::make_shared<Buffer>(desc, BufferType::constant);
    }

    void LightSystem::Update()
    {
        auto ecs = engine.GetECS();

        std::vector<PointLight> lightData;
        auto lightsView = ecs->GetRegistry().view<PointLight, Transform>();

        for (auto [entity, pointLight, transform] : lightsView.each())
        {
            PointLight gpuLight{};
            gpuLight.position = transform.GetPosition();
            gpuLight.colorIntensity = pointLight.colorIntensity;
            lightData.emplace_back(gpuLight);
        }

        m_pointLightCount = static_cast<uint32_t>(lightData.size());
        if (!lightData.empty()) { m_pointLightBuffer->Allocate(lightData.data()); }
    }
} // namespace Wild
