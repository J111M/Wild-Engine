#include "Renderer/Passes/PostProcessPass.hpp"
#include "Renderer/Passes/SkyboxPass.hpp"

namespace Wild
{
    PostProcessPass::PostProcessPass() {}

    void PostProcessPass::Add(Renderer& renderer, RenderGraph& rg) { VolumetricsPass(renderer, rg); }

    void PostProcessPass::Update(const float dt) {}

    void PostProcessPass::VolumetricsPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<VolumetricPassData>();
        auto* skyboxData = rg.GetPassData<VolumetricPassData, SkyPassData>();

        passData->finalTexture //= skyboxData->finalTexture;
        passData->depthTexture = skyboxData->depthTexture;

        rg.AddPass<VolumetricPassData>(
            "Volumetrics pass", PassType::Compute, [&renderer, this](VolumetricPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/PostProcess/Volumetrics.slang");

                std::vector<Uniform> uniforms;
                Uniform instanceCountBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(instanceCountBuffer);

                auto& pipeline = renderer.GetOrCreatePipeline("Volumetrics pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender();

                auto context = engine.GetGfxContext();
                UINT frameIndex = context->GetBackBufferIndex();
                list.SetUnorderedAccessView(0, m_instanceCountBuffer[frameIndex].get());

                list.GetList()->Dispatch(1, 1, 1);
                list.EndRender();
            });
    }
} // namespace Wild
