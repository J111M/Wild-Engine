#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Renderer/RenderGraph/TransientResourceCache.hpp"

#include "Renderer/Passes/DeferredPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"
#include "Renderer/Passes/OceanPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Passes/ProceduralTerrainPass.hpp"
#include "Renderer/Passes/SkyboxPass.hpp"
#include "Systems/GrassManager.hpp"

#include "Core/Camera.hpp"

#include "Tools/Log.hpp"

namespace Wild
{
    Renderer::Renderer()
    {
        auto gfxContext = engine.GetGfxContext();

        m_resourceCache = std::make_shared<TransientResourceCache>();

        m_renderFeatures.emplace_back(std::make_unique<ProceduralTerrainPass>());
        m_renderFeatures.emplace_back(std::make_unique<GrassPass>());
        m_renderFeatures.emplace_back(std::make_unique<DeferredPass>());
        m_renderFeatures.emplace_back(std::make_unique<PbrPass>());
        m_renderFeatures.emplace_back(std::make_unique<OceanPass>());
        m_renderFeatures.emplace_back(
            std::make_unique<SkyPass>("Assets/Textures/Skybox/kloofendal_48d_partly_cloudy_puresky_4k.hdr"));
    }

    Renderer::~Renderer() {}

    void Renderer::Update(const float dt)
    {
        for (auto& feature : m_renderFeatures)
        {
            feature->Update(dt);
        }
    }

    void Renderer::Render(CommandList& list, float deltaTime)
    {
        auto gfxContext = engine.GetGfxContext();

        RenderGraph rg = RenderGraph(*m_resourceCache);

        for (auto& feature : m_renderFeatures)
        {
            feature->Add(*this, rg);
        }

        rg.Compile();
        rg.Execute();

        // Copy final image over
        PipelineStateSettings settings{};
        settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/VertCopyRT.slang");
        settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/FragCopyRT.slang");
        settings.DepthStencilState.DepthEnable = false;
        settings.RasterizerState.WindingMode = WindingOrder::Clockwise;

        std::vector<Uniform> uniforms;

        {
            Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};

            CD3DX12_DESCRIPTOR_RANGE srvRange{};
            srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
            uni.ranges.emplace_back(srvRange);

            uniforms.emplace_back(uni);
        }

        {
            Uniform uni{0, 0, RootParams::RootResourceType::StaticSampler};
            uniforms.emplace_back(uni);
        }

        auto& pipeline = GetOrCreatePipeline("Copy pass", PipelineStateType::Graphics, settings, uniforms);

        compositeTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        list.SetPipelineState(pipeline);
        list.BeginRender({gfxContext->GetRenderTarget().get()}, {ClearOperation::Store}, nullptr, DSClearOperation::Store);
        list.GetList()->SetGraphicsRootDescriptorTable(0, compositeTexture->GetSrv()->GetGpuHandle());
        list.GetList()->DrawInstanced(3, 1, 0, 0);
        list.EndRender();
    }

    bool Renderer::HasPipelineInCache(const std::string& key)
    {
        // Count return 1 if a key is availiable
        return m_pipelineCache.find(key) != m_pipelineCache.end();
    }

    std::shared_ptr<PipelineState> Renderer::GetOrCreatePipeline(const std::string& key, PipelineStateType Type,
                                                                 const PipelineStateSettings& settings,
                                                                 const std::vector<Uniform>& uniforms)
    {
        if (HasPipelineInCache(key)) return m_pipelineCache.at(key);

        // If the unordered map doesn't contain a pipeline at hash value create a new one
        auto it = m_pipelineCache.emplace(key, std::make_shared<PipelineState>(Type, settings, uniforms)).first;

        // It contains an std::pair of hash key and pipeline state
        return it->second;
    }

    std::shared_ptr<PipelineState> Renderer::GetPipeline(const std::string& key)
    {
        if (m_pipelineCache.find(key) != m_pipelineCache.end()) { return m_pipelineCache.at(key); }
        else
        {
            WD_WARN("Invalid pipeline found of type key: &s", key.c_str());
        }

        return nullptr;
    }

    void Renderer::FlushResources()
    {
        if (m_resourceCache) m_resourceCache->Flush();
    }
} // namespace Wild
