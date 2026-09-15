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

        // Also orders this pass after the DDGI probe trace pass, which is what owns the atlas pair
        auto* ddgiData = rg.GetPassData<ProbeDebugPassData, DDGIPassData>();

        passData->targetTexture = lineData->debugTexture;
        passData->depthTexture = deferredData->depthTexture;

        rg.AddPass<ProbeDebugPassData>(
            "Probe debug pass",
            PassType::Graphics,
            [&renderer, ddgiData, this](const ProbeDebugPassData& passData, CommandList& list) {
                if (!drawProbes) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                // The shader resolves the atlas straight from the descriptor heap
                if (!engine.GetGfxContext()->GetCapabilities().CheckResourceBindingSupport(ResourceBindingSupport::Tier3))
                {
                    static bool bindingTierWarned = false;
                    if (!bindingTierWarned)
                    {
                        WD_WARN("Probe debug pass needs resource binding tier 3, skipping.");
                        bindingTierWarned = true;
                    }
                    return;
                }

                Texture* irradianceAtlas = ddgiData->iradianceTexture[ddgiData->frameParity];
                auto irradianceSrv = irradianceAtlas ? irradianceAtlas->GetSrv() : nullptr;

                if (!irradianceSrv)
                {
                    static bool viewWarned = false;
                    if (!viewWarned)
                    {
                        WD_WARN("Probe debug pass has no irradiance atlas SRV, skipping.");
                        viewWarned = true;
                    }
                    return;
                }

                // TODO improve check
                if (ddgiData->probeDataView == INVALID_HEAP_INDEX)
                {
                    static bool probeDataWarned = false;
                    if (!probeDataWarned)
                    {
                        WD_WARN("Probe debug pass has no DDGI probe data, is DDGI switched off? Skipping.");
                        probeDataWarned = true;
                    }
                    return;
                }

                PipelineStateSettings settings{};
                settings.shaderState.vertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/ProbeDebugVert.slang");
                settings.shaderState.fragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/ProbeDebugFrag.slang");

                settings.rasterizerState.cullMode = CullMode::Back;
                settings.renderTargetsFormat.push_back(passData.targetTexture->GetDesc().format);

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(ProbeDebugRootConstants)};
                uniforms.emplace_back(rootConstant);

                // Probe positions structured buffer, indexed with the instance id
                Uniform probeBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(probeBuffer);

                Uniform clampSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                clampSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                clampSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                clampSampler.samplerState.addressModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                clampSampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(clampSampler);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Probe debug pass", PipelineStateType::Graphics, settings, uniforms);

                list.SetPipelineState(pipeline);

                m_rc.probeDataView = ddgiData->probeDataView;
                m_rc.irradianceExposure = irradianceExposure;

                // A graphics pass reads it, so the trace pass' NON_PIXEL_SHADER_RESOURCE is not enough
                irradianceAtlas->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

                list.BeginRender({passData.targetTexture},
                                 {ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store,
                                 "Probe debug pass");

                list.SetRootConstant<ProbeDebugRootConstants>(0, m_rc);
                list.SetShaderResourceView(1, probeSystem->GetProbeBuffer().get());

                list.GetList()->DrawInstanced(12 * 8 * 6, probeSystem->GetProbeCount(), 0, 0);

                list.EndRender();
            });
    }
} // namespace Wild
