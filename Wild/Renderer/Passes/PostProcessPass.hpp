#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct VolumetricPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct VolumetricRC
    {
        glm::vec2 textureSize{};
        glm::vec2 nearFar{};

        uint32_t srcTextureView{};
        uint32_t depthView{};
        uint32_t irradianceView{};
        uint32_t numOfPointLights{};

        uint32_t stepCount = 64;
        float stepSize = 0.5f;
        float scatteringDensity = 0.3f;
        float density = 0.03f;

        glm::vec4 scatteringColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        glm::vec4 sunColorIntensity = glm::vec4(1.0, 0.95, 0.8, 1.0f);

        float anisotropy = 0.3f;
        float pad[3];
    };

    struct FinalPostProcessPassData
    {
        Texture* finalTexture;
    };

    struct PostProcessRC
    {
        glm::vec2 textureSize{};
        uint32_t srcTextureView{};
        uint32_t pad{};
    };

    struct SceneBuffer
    {
        glm::mat4 inverseProj;
        glm::mat4 inverseView;
        glm::vec3 cameraPosition;
        float pad;
        glm::vec3 lightDirection;
    };

    class PostProcessPass : public RenderFeature
    {
      public:
        PostProcessPass();
        ~PostProcessPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        void VolumetricsPass(Renderer& renderer, RenderGraph& rg);
        void FinalPostProcessPass(Renderer& renderer, RenderGraph& rg);

        VolumetricRC m_vrc{};
        SceneBuffer m_sceneData{};
        std::unique_ptr<Buffer> m_sceneDataBuffer[BACK_BUFFER_COUNT];

        PostProcessRC m_pprc{};
    };
} // namespace Wild
