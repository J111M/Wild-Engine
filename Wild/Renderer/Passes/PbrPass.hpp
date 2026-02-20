#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct PbrPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct InverseCamera
    {
        glm::mat4 inverseView{};
        glm::mat4 inverseProj{};
    };

    struct PBRData
    {
        glm::vec3 cameraPosition{};
        glm::vec3 lightDirection = glm::vec4(-0.3, 14.0, -2.5, 1.0f);
        uint32_t viewMode = 0;
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

        std::unique_ptr<Buffer> m_inverseCamera[BACK_BUFFER_COUNT];
        std::unique_ptr<Buffer> m_pbrDataBuffer[BACK_BUFFER_COUNT];
        std::unique_ptr<Buffer> m_environmentData;
    };
} // namespace Wild
