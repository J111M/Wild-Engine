#include "Systems/LightSystem.hpp"

#include "Renderer/Resources/LightTypes.hpp"

#include <vector>

namespace Wild
{
    LightSystem::LightSystem()
    {
        BufferDesc desc{};
        desc.size = sizeof(PointLight) * MAX_POINT_LIGHTS;
        desc.usage = BufferUsage::Constant;
        desc.access = MemoryAccess::CpuToGpu;
        m_pointLightBuffer = std::make_shared<GPUBuffer>(desc);
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
