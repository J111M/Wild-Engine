#pragma once

#include "Renderer/Passes/DDGIPass.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct ProbeDebugRootConstants
    {
        glm::mat4 projView{};

        float probeScale = 0.25f;
        uint32_t probeDataView{INVALID_HEAP_INDEX};
        float irradianceExposure{20.0f};
        float padding{};
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

        float irradianceExposure = 20.0f;

      private:
        ProbeDebugRootConstants m_rc{};
    };
} // namespace Wild
