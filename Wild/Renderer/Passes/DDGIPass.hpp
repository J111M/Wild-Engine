#pragma once

namespace Wild
{
    struct DDGIRC
    {
    
    };

    struct DDGIPassData
    {
        float foo;
    };

    class DDGIPass : public RenderFeature
    {
      public:
        DDGIPass();
        ~DDGIPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
    };
} // namespace Wild
