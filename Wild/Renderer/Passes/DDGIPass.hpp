#pragma once

#include "Systems/ProbeSystem.hpp"

namespace Wild
{
    struct DDGIRC
    {
        glm::mat4 inverseView{};
        glm::mat4 inverseProj{};

        uint32_t environmentView{};
    };

    struct DDGIPassData
    {
        Texture* finalTex;
    };

    struct ProbeDebugPassData
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
        void AddDebugProbePass(Renderer& renderer, RenderGraph& rg);

      private:
        std::unique_ptr<ProbeSystem> m_probeSystem;

        DDGIRC m_rc{};
    };
} // namespace Wild
