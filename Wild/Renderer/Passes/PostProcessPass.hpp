#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    /// Pre computed volumetric noise pass
    struct VolumetricNoiseRC
    {
        float textureSize;
        float cellCount = 4;
    };

    struct VolumetricNoisePassData
    {
        Texture* volumetricNoise; // Worley combined with perlin noise
    };

    /// Volumetric ray march pass
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

        glm::vec4 scatteringColor = glm::vec4(1.0f, 0.854f, 0.67f, 1.0f);

        glm::vec4 sunColorIntensity = glm::vec4(1.0, 0.95, 0.8, 1.0f);

        float anisotropy = 0.875f;
        float biasValue{};
        float noiseScale = 50.0f;
        float pad;

        uint32_t shadowMapView[4]; // Max of 4 shadow maps

        glm::vec4 windDirectionTime = glm::vec4(0.5, 0.2, 0.5, 0);

        uint32_t noiseView{};
    };

    /// Final post process pass HDR and other effects
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
        glm::mat4 viewSpace;
        glm::vec3 cameraPosition;
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
        void VolumetricNoisePass(Renderer& renderer, RenderGraph& rg);
        void VolumetricsPass(Renderer& renderer, RenderGraph& rg);
        void FinalPostProcessPass(Renderer& renderer, RenderGraph& rg);

        // Volumetric noise
        VolumetricNoiseRC m_noiseRC{};
        bool m_recomputeVolumetricNoise = true;

        // Volumetric data
        VolumetricRC m_volumetricRC{};
        SceneBuffer m_sceneData{};
        std::unique_ptr<Buffer> m_sceneDataBuffer[BACK_BUFFER_COUNT];

        float m_fogTime{};
        float m_windSpeed = 0.5f;

        // Post process data
        PostProcessRC m_postProcessRC{};
    };
} // namespace Wild
