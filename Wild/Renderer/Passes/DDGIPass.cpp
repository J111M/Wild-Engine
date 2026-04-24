#include "Renderer/Passes/DDGIPass.hpp"
#include "Renderer/Passes/DebugLinePass.hpp"

namespace Wild
{
    DDGIPass::DDGIPass() {}

    void DDGIPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DDGIPassData>();
        auto* linePass = rg.GetPassData<DDGIPassData, DebugLinePassData>();

        TextureDesc desc;
        desc.width = engine.GetGfxContext()->GetWidth();
        desc.height = engine.GetGfxContext()->GetHeight();
        desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
        desc.name = "ddgifinaltexture";
        desc.usage = TextureDesc::gpuOnly;
        desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
        passData->finalTex = rg.CreateTransientTexture("ddgifinaltexture", desc);

        rg.AddPass<DDGIPassData>(
            "DDGI pass", PassType::Raytracing, [&renderer, desc, this](DDGIPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};

                settings.ShaderState.rayTracingShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/ddgiRaytrace.slang");

                settings.raytracingState.payloadSize = sizeof(float) * 3;
                settings.raytracingState.attributeSize = sizeof(float) * 2;

                settings.raytracingState.rayRecursionDepth = 1;

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DDGIRC)};
                uniforms.emplace_back(rootConstant);

                Uniform accelerationStructure{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(accelerationStructure);

                // Output render target
                Uniform outputTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE outputUAVRange{};
                outputUAVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                outputTexture.ranges.emplace_back(outputUAVRange);
                uniforms.emplace_back(outputTexture);

                auto& pipeline = renderer.GetOrCreatePipeline("DDGI Pass", PipelineStateType::Raytracing, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Dynamic Diffuse Global Illumination pass");

                list.SetRootConstant<DDGIRC>(0, m_rc);
                list.GetList()->SetComputeRootShaderResourceView(1, engine.GetAccelerationStructureManager()->GetTLASAddress());
                list.SetUnorderedAccessView(2, passData.finalTex);

                // Modify to probe size
                list.Dispatch(desc.width, desc.height, 1);

                list.EndRender();

                // renderer.compositeTexture = passData.finalTex;
            });
    }

    void DDGIPass::Update(const float dt)
    {
        Camera* camera = GetActiveCamera();

        if (camera)
        {
            m_rc.inverseView = glm::inverse(camera->GetView());
            m_rc.inverseProj = glm::inverse(camera->GetProjection());
        }
    }
} // namespace Wild
