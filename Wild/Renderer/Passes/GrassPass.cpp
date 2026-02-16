#include "Renderer/Passes/GrassPass.hpp"
#include "Renderer/Passes/ProceduralTerrainPass.hpp"

#include <glm/gtc/matrix_access.hpp>

namespace Wild
{
    GrassPass::GrassPass()
    {
        auto& context = engine.GetGfxContext();
        m_meshShaderSupported = context->GetCapabilities().SupportsMeshShaders();

        // The mesh shader variant is not implemented yet
        m_meshShaderSupported = false;

        if (m_meshShaderSupported)
            m_meshShaderGrass = std::make_unique<MeshShaderGrass>();
        else
            m_gpuDrivenGrass = std::make_unique<GPUDriveGrass>();
    }

    void GrassPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        // Check mesh shader support
        if (m_meshShaderSupported)
            m_meshShaderGrass->Add(renderer, rg);
        else
            m_gpuDrivenGrass->Add(renderer, rg);
    }

    void GrassPass::Update(const float dt)
    {
        if (m_meshShaderSupported)
            m_meshShaderGrass->Update(dt);
        else
            m_gpuDrivenGrass->Update(dt);
    }

} // namespace Wild
