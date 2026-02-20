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
        float aspect;
        uint32_t srcTextureView{};
        uint32_t depthView{};
    };

    struct SceneBuffer
    {
        glm::mat4 inverseProj;
        glm::mat4 inverseView;
        glm::vec3 cameraPosition;
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

        VolumetricRC m_vrc;
        std::unique_ptr<Buffer> m_sceneDataBuffer[BACK_BUFFER_COUNT];
    };
} // namespace Wild
