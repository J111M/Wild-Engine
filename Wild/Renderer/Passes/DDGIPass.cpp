#include "Renderer/Passes/DDGIPass.hpp"

namespace Wild
{
    DDGIPass::DDGIPass() {}

    void DDGIPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DDGIPassData>();

        rg.AddPass<DDGIPassData>("DDGI pass", PassType::Raytracing, [&renderer, this](DDGIPassData& passData, CommandList& list) {
            PipelineStateSettings settings{};
            settings.ShaderState.rayTracingShader =
                engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/ddgiRaytrace.slang");

            settings.raytracingState.payloadSize = 1;
            settings.raytracingState.attributeSize = 1;

            settings.raytracingState.rayRecursionDepth = 1;

            std::vector<Uniform> uniforms;

            Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DDGIRC)};
            uniforms.emplace_back(rootConstant);

            auto& pipeline = renderer.GetOrCreatePipeline("DDGI Pass", PipelineStateType::Raytracing, settings, uniforms);
            list.SetPipelineState(pipeline);
            list.BeginRender("Dynamic Diffuse Global Illumination pass");

            list.SetRootConstant<DDGIRC>(0, DDGIRC{});

            // Modify to probe size
            list.Dispatch(1920, 1080, 1);

            list.EndRender();
        });
    }

    void DDGIPass::Update(const float dt) {}
} // namespace Wild
