#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    class PostProcessPass : RenderFeature
    {
      public:
        PostProcessPass();
        ~PostProcessPass();

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
    };
} // namespace Wild
