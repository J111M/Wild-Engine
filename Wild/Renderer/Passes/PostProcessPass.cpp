#include "Renderer/Passes/PostProcessPass.hpp"
#include "Renderer/Passes/SkyboxPass.hpp"

namespace Wild
{
    PostProcessPass::PostProcessPass()
    {
        BufferDesc desc{};
        desc.bufferSize = sizeof(SceneBuffer);
        for (size_t i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_sceneDataBuffer[i] = std::make_unique<Buffer>(desc, BufferType::constant);
        }
    }

    void PostProcessPass::Add(Renderer& renderer, RenderGraph& rg) { VolumetricsPass(renderer, rg); }

    void PostProcessPass::Update(const float dt)
    {
        auto ecs = engine.GetECS();
        auto& cameras = ecs->View<Camera>();

        SceneBuffer sceneData{};

        // Loop over all camera's TODO make the code run for each camera entity
        for (auto& cameraEntity : cameras)
        {
            if (ecs->HasComponent<Camera>(cameraEntity))
            {
                auto& cam = ecs->GetComponent<Camera>(cameraEntity);

                sceneData.inverseView = glm::inverse(cam.GetView());
                sceneData.inverseProj = glm::inverse(cam.GetProjection());
                sceneData.cameraPosition = cam.GetPosition();
                m_vrc.nearFar = cam.GetNearFar();
                m_vrc.aspect = cam.GetAspect();
            }
        }

        int frameIndex = engine.GetGfxContext()->GetBackBufferIndex();
        m_sceneDataBuffer[frameIndex]->Allocate(&sceneData);
    }

    void PostProcessPass::VolumetricsPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<VolumetricPassData>();
        auto* skyboxData = rg.GetPassData<VolumetricPassData, SkyPassData>();

        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.Height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
            desc.name = "Post Process Texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
            passData->finalTexture = rg.CreateTransientTexture("Post Process Texture", desc);
        }

        passData->depthTexture = skyboxData->depthTexture;

        rg.AddPass<VolumetricPassData>(
            "Volumetrics pass",
            PassType::Compute,
            [&renderer, skyboxData, this](VolumetricPassData& passData, CommandList& list) {
                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/PostProcess/Volumetrics.slang");

                std::vector<Uniform> uniforms;
                uniforms.reserve(5);

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(VolumetricRC)};
                uniforms.emplace_back(rootConstant);

                Uniform cameraBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(cameraBuffer);

                // Output render target
                Uniform outputTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE outputUAVRange{};
                outputUAVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                outputTexture.ranges.emplace_back(outputUAVRange);
                uniforms.emplace_back(outputTexture);

                Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE textures{};
                textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                uni.ranges.emplace_back(textures);

                CD3DX12_DESCRIPTOR_RANGE cubeMaps{};
                cubeMaps.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              1,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for cube maps
                uni.ranges.emplace_back(cubeMaps);

                uniforms.emplace_back(uni);

                Uniform pointSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                pointSampler.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                pointSampler.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(pointSampler);

                int frameIndex = engine.GetGfxContext()->GetBackBufferIndex();

                auto& pipeline = renderer.GetOrCreatePipeline("Volumetrics pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender();

                skyboxData->finalTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                m_vrc.srcTextureView = skyboxData->finalTexture->GetSrv()->BindlessView();
                m_vrc.depthView = passData.depthTexture->GetSrv()->BindlessView();

                m_vrc.textureSize = glm::vec2(passData.finalTexture->Width(), passData.finalTexture->Height());

                list.SetRootConstant<VolumetricRC>(0u, m_vrc);
                list.SetConstantBufferView(1u, m_sceneDataBuffer[frameIndex].get());
                list.SetUnorderedAccessView(2u, passData.finalTexture);
                list.SetBindlessHeap(3u);

                // Thread group size of 8 to reduce occupancy
                list.GetList()->Dispatch(
                    (passData.finalTexture->Width() + 7u) / 8u, (passData.finalTexture->Height() + 7u) / 8u, 1u);
                list.EndRender();

                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                renderer.compositeTexture = passData.finalTexture;
            });
    }
} // namespace Wild
