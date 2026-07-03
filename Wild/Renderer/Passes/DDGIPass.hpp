#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct DDGIRootConstants
    {
        glm::mat4 inverseView{};
        glm::mat4 inverseProj{};

        uint32_t environmentView{};
    };

    struct DDGIPassData
    {
        Texture* finalTex;
    };

    class DDGIPass : public RenderFeature
    {
      public:
        DDGIPass();
        ~DDGIPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void AddProbeTracePass(Renderer& renderer, RenderGraph& rg);

      private:
        DDGIRootConstants m_rc{};
    };
} // namespace Wild
