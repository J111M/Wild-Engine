#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct ProbeDebugRootConstants
    {
        glm::mat4 projView{};
        float probeScale = 0.25f;
        float padding[3]{};
    };

    struct ProbeDebugPassData
    {
        Texture* targetTexture;
        Texture* depthTexture;
    };

    class ProbeDebugPass : public RenderFeature
    {
      public:
        ProbeDebugPass() {};
        ~ProbeDebugPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        bool drawProbes = false;

      private:
        ProbeDebugRootConstants m_rc{};
    };
} // namespace Wild
