#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct RootConstant
    {
        glm::mat4 matrix{};
        glm::mat4 invMatrix{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};
    };

    struct DeferredPassData
    {
        Texture *albedoRoughnessTexture;
        Texture *normalMetallicTexture;
        Texture *emissiveTexture;
        Texture *depthTexture;
    };

    class DeferredPass : public RenderFeature
    {
      public:
        DeferredPass();
        ~DeferredPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        RootConstant m_rc;
        std::shared_ptr<PipelineState> m_pipeline{};

        std::unique_ptr<Texture> m_texture;
    };
} // namespace Wild
