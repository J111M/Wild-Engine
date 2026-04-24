#pragma once

namespace Wild
{
    struct DDGIRC
    {
        glm::mat4 inverseView{};
        glm::mat4 inverseProj{};
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

      private:
        DDGIRC m_rc{};
    };
} // namespace Wild
