#pragma once

#include "Renderer/Passes/ProceduralGrass/GPUDrivenGrass.hpp"
#include "Renderer/Passes/ProceduralGrass/MeshShaderGrass.hpp"

namespace Wild
{

    class GrassPass : public RenderFeature
    {
      public:
        GrassPass();
        ~GrassPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        std::unique_ptr<GPUDriveGrass> m_gpuDrivenGrass;
        std::unique_ptr<MeshShaderGrass> m_meshShaderGrass;

        bool m_meshShaderSupported = false;
    };
} // namespace Wild
