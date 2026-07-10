#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Systems/ProbeSystem.hpp"

namespace Wild
{
    struct DDGIRootConstants
    {
        glm::vec4 randomRotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (xyz axis*sin(half), w = cos(half))

        glm::vec4 lightDirection{0.0f, -1.0f, 0.0f, 0.0f};     // xyz = direction the light travels, w unused
        glm::vec4 lightColorIntensity{1.0f, 1.0f, 1.0f, 1.0f}; // rgb = color, a = intensity

        glm::vec4 probeOrigin{};                        // xyz = grid origin, w unused
        glm::vec4 probeSpacing{1.0f, 1.0f, 1.0f, 0.0f}; // xyz = grid spacing, w unused
        glm::ivec4 probeCounts{};                       // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

        uint32_t raysPerProbe{64};
        float maxRayDistance{1000.0f};
        float intensity{1.0f};
        uint32_t environmentView{};

        uint32_t numPointLights{};
    };

    struct DDGIBlendRootConstants
    {
        float hysteresis{0.97f};
        uint32_t raysPerProbe{64};
        uint32_t maxRaysPerProbe{};
        uint32_t probeCount{};
    };

    struct DDGIPassData
    {
        Texture* radianceTexture{};
        Texture* visibilityTexture{};
    };

    struct DDGIBlendPassData
    {
    };

    class DDGIPass : public RenderFeature
    {
      public:
        DDGIPass();
        ~DDGIPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void AddProbeTracePass(Renderer& renderer, RenderGraph& rg);
        void AddProbeBlendPass(Renderer& renderer, RenderGraph& rg);

        bool enabled = false;

      private:
        DDGIRootConstants m_rc{};
        DDGIBlendRootConstants m_blendRc{};

        int m_raysPerProbe = 64;
        float m_hysteresis = 0.97f;
        float m_maxRayDistance = 1000.0f;
        float m_intensity = 1.0f;

        bool m_randomRayRotation = true;
        bool m_freezeUpdates = false;
    };
} // namespace Wild
