#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#include "Renderer/Passes/CascadedShadowPass.hpp"

#include "Renderer/Resources/LightTypes.hpp"

namespace Wild
{
#define MAX_POINT_LIGHTS 15;

    struct PbrPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
        std::shared_ptr<Buffer> pointlights;
        uint32_t numOfPointLights{};
    };

    struct CameraBuffer
    {
        glm::mat4 inverseView{};
        glm::mat4 inverseProj{};
        glm::mat4 viewSpace{};
        float cameraFar{};
    };

    struct PBRData
    {
        glm::vec3 cameraPosition{};
        glm::vec4 lightDirectionIntensity = glm::vec4(-0.3, 14.0, -2.5, 1.0f);
        uint32_t viewMode = 0;
        uint32_t numOfPointLights{};
    };

    struct EnvironmentData
    {
        uint32_t irradianceView{};
        uint32_t specularView{};
        uint32_t brdfView{};
    };

    struct PbrRootConstant
    {
        uint32_t emissiveView{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t depthView{};
        uint32_t shadowMapView[SHADOWMAP_CASCADES];
        float depthBias = 0.05f;
    };

    class PbrPass : public RenderFeature
    {
      public:
        PbrPass();
        ~PbrPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        PbrRootConstant m_rc{};
        PBRData m_pbrData{};

        CameraBuffer m_camData{};

        std::unique_ptr<Buffer> m_cameraBuffer[BACK_BUFFER_COUNT];
        std::unique_ptr<Buffer> m_pbrDataBuffer[BACK_BUFFER_COUNT];

        std::shared_ptr<Buffer> m_pointLightsBuffer;

        std::unique_ptr<Buffer> m_environmentData;
    };
} // namespace Wild
