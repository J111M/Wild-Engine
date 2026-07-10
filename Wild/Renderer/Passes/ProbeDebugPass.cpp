#include "Renderer/Passes/ProbeDebugPass.hpp"
#include "Renderer/Passes/DebugLinePass.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

#include "Systems/ProbeSystem.hpp"

namespace Wild
{
    void ProbeDebugPass::Update(const float dt)
    {
        Camera* camera = GetActiveCamera();
        if (camera) { m_rc.projView = camera->GetProjection() * camera->GetView(); }
    }

    void ProbeDebugPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<ProbeDebugPassData>();
        auto* lineData = rg.GetPassData<ProbeDebugPassData, DebugLinePassData>();
        auto* deferredData = rg.GetPassData<ProbeDebugPassData, DeferredPassData>();

        passData->targetTexture = lineData->debugTexture;
        passData->depthTexture = deferredData->depthTexture;

        rg.AddPass<ProbeDebugPassData>(
            "Probe debug pass", PassType::Graphics, [&renderer, this](const ProbeDebugPassData& passData, CommandList& list) {
                if (!drawProbes) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/ProbeDebugVert.slang");
                settings.ShaderState.FragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/ProbeDebugFrag.slang");

                settings.RasterizerState.CullMode = CullMode::Back;
                settings.renderTargetsFormat.push_back(passData.targetTexture->GetDesc().format);

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(ProbeDebugRootConstants)};
                uniforms.emplace_back(rootConstant);

                // Probe positions structured buffer, indexed with the instance id
                Uniform probeBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(probeBuffer);

                // Probe irradiance structured buffer, indexed with the instance id
                Uniform probeIrradianceBuffer{1, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(probeIrradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Probe debug pass", PipelineStateType::Graphics, settings, uniforms);

                list.SetPipelineState(pipeline);

                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

                list.BeginRender({passData.targetTexture},
                                 {ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store,
                                 "Probe debug pass");

                list.SetRootConstant<ProbeDebugRootConstants>(0, m_rc);
                list.SetShaderResourceView(1, probeSystem->GetProbeBuffer().get());
                list.SetShaderResourceView(2, probeSystem->GetProbeIrradianceBuffer().get());

                list.GetList()->DrawInstanced(12 * 8 * 6, probeSystem->GetProbeCount(), 0, 0);

                list.EndRender();
            });
    }
} // namespace Wild
