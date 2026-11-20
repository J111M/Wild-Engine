#pragma once

#include "Renderer/Passes/DDGIPass.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    // Camera matrices come from the scene data buffer
    struct ProbeDebugRootConstants
    {
        float probeScale = 0.15f;
        uint32_t probeDataView{INVALID_HEAP_INDEX};
        float irradianceExposure{20.0f};
        int32_t debugCascade{-1};
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

        float probeScale = 0.15f;

        // -1 draws every cascade
        int debugCascade = 0;

      private:
        ProbeDebugRootConstants m_rc{};
    };
} // namespace Wild
