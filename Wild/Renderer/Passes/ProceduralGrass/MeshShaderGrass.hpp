#pragma once

namespace Wild
{
    class MeshShaderGrass : public RenderFeature
    {
      public:
        MeshShaderGrass();
        ~MeshShaderGrass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
    };
} // namespace Wild
