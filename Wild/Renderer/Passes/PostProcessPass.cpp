#include "Renderer/Passes/PostProcessPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"
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

    void PostProcessPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        VolumetricNoisePass(renderer, rg);
        VolumetricsPass(renderer, rg);
        FinalPostProcessPass(renderer, rg);
    }

    void PostProcessPass::Update(const float dt)
    {
        m_fogTime += m_windSpeed * dt;

        auto ecs = engine.GetECS();
        Camera* camera = GetActiveCamera();

        if (camera)
        {
            m_sceneData.inverseView = glm::inverse(camera->GetView());
            m_sceneData.inverseProj = glm::inverse(camera->GetProjection());
            m_sceneData.viewSpace = camera->GetView();
            m_sceneData.cameraPosition = camera->GetPosition();
            m_volumetricRC.nearFar = camera->GetNearFar();
        }

        m_sceneData.lightDirection = glm::vec3(-0.3, 14.0, -2.5);

        engine.GetImGui()->AddPanel("Volumetric Fog Settings", [this]() {
            ImGui::SliderInt("Step Count", reinterpret_cast<int*>(&m_volumetricRC.stepCount), 1, 256);
            ImGui::SliderFloat("Step Size", &m_volumetricRC.stepSize, 0.1f, 2.0f);
            ImGui::SliderFloat("Scattering Density", &m_volumetricRC.scatteringDensity, 0.0f, 2.0f);
            ImGui::SliderFloat("Density", &m_volumetricRC.density, 0.0f, 0.2f);
            ImGui::ColorEdit3("Scattering Color", &m_volumetricRC.scatteringColor[0]);
            ImGui::ColorEdit3("Sun Color", &m_volumetricRC.sunColorIntensity[0]);
            ImGui::SliderFloat("Sun Intensity", &m_volumetricRC.sunColorIntensity.w, 0.001f, 2.0f);
            ImGui::SliderFloat("Anisotropy", &m_volumetricRC.anisotropy, 0.001f, 0.99f);
            ImGui::SliderFloat3("Wind direction", &m_volumetricRC.windDirectionTime[0], -1.0f, 1.0f);
            ImGui::SliderFloat("Wind speed", &m_windSpeed, 0.0f, 5.0f);
            ImGui::SliderFloat("Wind noise scale", &m_volumetricRC.noiseScale, 1.0f, 500.0f);
        });

        m_volumetricRC.windDirectionTime.a = m_fogTime;

        int frameIndex = engine.GetGfxContext()->GetBackBufferIndex();
        m_sceneDataBuffer[frameIndex]->Allocate(&m_sceneData);
    }

    void PostProcessPass::VolumetricNoisePass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<VolumetricNoisePassData>();

        TextureDesc desc;
        desc.width = 128;
        desc.height = 128;
        desc.depthOrArray = 128;
        desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.name = "Volumetric noise texture";
        desc.usage = TextureDesc::gpuOnly;
        desc.type = TextureType::TEXTURE_3D;
        desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
        desc.shouldStayInCache = true;
        passData->volumetricNoise = rg.CreateTransientTexture("Volumetric noise texture", desc);

        rg.AddPass<VolumetricNoisePassData>(
            "Precompute volumetric noise pass",
            PassType::Compute,
            [&renderer, desc, this](VolumetricNoisePassData& passData, CommandList& list) {
                if (m_recomputeVolumetricNoise)
                {
                    m_noiseRC.textureSize = desc.width;
                    m_noiseRC.cellCount = 4;

                    PipelineStateSettings settings{};
                    settings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/PostProcess/VolumetricNoise.slang");

                    std::vector<Uniform> uniforms;

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(VolumetricNoiseRC)};
                    uniforms.emplace_back(rootConstant);

                    Uniform volumetricNoiseTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    volumetricNoiseTexture.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(volumetricNoiseTexture);

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Precompute volumetric noise pass", PipelineStateType::Compute, settings, uniforms);
                    list.SetPipelineState(pipeline);
                    list.BeginRender("Precompute volumetric noise pass");

                    list.SetRootConstant<VolumetricNoiseRC>(0, m_noiseRC);
                    list.SetUnorderedAccessView(1, passData.volumetricNoise);

                    // Use 3D texture dimension
                    list.GetList()->Dispatch((desc.width + 7) / 8, (desc.height + 7) / 8, (desc.depthOrArray + 7) / 8);

                    list.EndRender();

                    passData.volumetricNoise->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    m_recomputeVolumetricNoise = false;
                }
            });
    }

    void PostProcessPass::VolumetricsPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<VolumetricPassData>();
        auto* skyboxData = rg.GetPassData<VolumetricPassData, SkyPassData>();
        auto* pbrData = rg.GetPassData<VolumetricPassData, PbrPassData>();
        auto* shadowData = rg.GetPassData<VolumetricPassData, CsmPassData>();
        auto* volumetricNoise = rg.GetPassData<VolumetricPassData, VolumetricNoisePassData>();

        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "Post Process Texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
            passData->finalTexture = rg.CreateTransientTexture("Post Process Texture", desc);
        }

        passData->depthTexture = skyboxData->depthTexture;

        rg.AddPass<VolumetricPassData>(
            "Volumetrics pass",
            PassType::Compute,
            [&renderer, skyboxData, pbrData, shadowData, volumetricNoise, this](VolumetricPassData& passData, CommandList& list) {
                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                renderer.irradianceMap->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                for (size_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
                {
                    // TODO batcht transition together to optimize
                    shadowData->shadowMap[cascade]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    m_volumetricRC.shadowMapView[cascade] = shadowData->shadowMap[cascade]->GetSrv()->BindlessView();
                }

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/PostProcess/Volumetrics.slang");

                std::vector<Uniform> uniforms;
                uniforms.reserve(6);

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(VolumetricRC)};
                uniforms.emplace_back(rootConstant);

                Uniform cameraBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(cameraBuffer);

                Uniform lightsBuffer{2, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(lightsBuffer);

                Uniform shadowBuffer{3, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(shadowBuffer);

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
                cubeMaps.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                uni.ranges.emplace_back(cubeMaps);

                CD3DX12_DESCRIPTOR_RANGE textures3D{};
                textures3D.Init(
                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 2, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                uni.ranges.emplace_back(textures3D);

                uniforms.emplace_back(uni);

                Uniform pointSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                pointSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                pointSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(pointSampler);

                Uniform shadowSampler{1, 0, RootParams::RootResourceType::StaticSampler};
                shadowSampler.samplerState.filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
                shadowSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                shadowSampler.samplerState.comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
                uniforms.emplace_back(shadowSampler);

                Uniform fogSampler{2, 0, RootParams::RootResourceType::StaticSampler};
                fogSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                fogSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                uniforms.emplace_back(fogSampler);

                int frameIndex = engine.GetGfxContext()->GetBackBufferIndex();

                auto& pipeline = renderer.GetOrCreatePipeline("Volumetrics pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Volumetric pass");

                skyboxData->finalTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                m_volumetricRC.srcTextureView = skyboxData->finalTexture->GetSrv()->BindlessView();
                m_volumetricRC.depthView = passData.depthTexture->GetSrv()->BindlessView();
                m_volumetricRC.numOfPointLights = pbrData->numOfPointLights;
                m_volumetricRC.noiseView = volumetricNoise->volumetricNoise->GetSrv()->BindlessView();

                if (renderer.irradianceMap) { m_volumetricRC.irradianceView = renderer.irradianceMap->GetSrv()->BindlessView(); }
                m_volumetricRC.textureSize = glm::vec2(passData.finalTexture->Width(), passData.finalTexture->Height());
                m_volumetricRC.biasValue = shadowData->biasValue;

                list.SetRootConstant<VolumetricRC>(0u, m_volumetricRC);
                list.SetConstantBufferView(1u, m_sceneDataBuffer[frameIndex].get());
                list.SetConstantBufferView(2u, pbrData->pointlights.get());
                list.SetConstantBufferView(3u, shadowData->directLightBuffer.get());
                list.SetUnorderedAccessView(4u, passData.finalTexture);
                list.SetBindlessHeap(5u);

                // Thread group size of 8 to reduce occupancy
                list.GetList()->Dispatch(
                    (passData.finalTexture->Width() + 7u) / 8u, (passData.finalTexture->Height() + 7u) / 8u, 1u);
                list.EndRender();

                renderer.irradianceMap->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            });
    }

    void PostProcessPass::FinalPostProcessPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<FinalPostProcessPassData>();
        auto* volumetricData = rg.GetPassData<FinalPostProcessPassData, VolumetricPassData>();

        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R10G10B10A2_UNORM;
            desc.name = "Final Post Process Assemble texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite |
                                                           TextureDesc::renderTarget);
            passData->finalTexture = rg.CreateTransientTexture("Final Post Process Assemble texture", desc);
        }

        rg.AddPass<FinalPostProcessPassData>(
            "Final post process pass",
            PassType::Compute,
            [&renderer, volumetricData, this](FinalPostProcessPassData& passData, CommandList& list) {
                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/PostProcess/FinalPostProcess.slang");

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(PostProcessRC)};
                uniforms.emplace_back(rootConstant);

                // Output render target
                Uniform outputTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE outputUAVRange{};
                outputUAVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                outputTexture.ranges.emplace_back(outputUAVRange);
                uniforms.emplace_back(outputTexture);

                Uniform bindlessTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE textures{};
                textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                bindlessTexture.ranges.emplace_back(textures);

                uniforms.emplace_back(bindlessTexture);

                Uniform pointSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                pointSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                pointSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(pointSampler);

                int frameIndex = engine.GetGfxContext()->GetBackBufferIndex();

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Final post process pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Post process and tonemap pass");

                m_postProcessRC.textureSize = glm::vec2(passData.finalTexture->Width(), passData.finalTexture->Height());
                m_postProcessRC.srcTextureView = volumetricData->finalTexture->GetSrv()->BindlessView();

                list.SetRootConstant<PostProcessRC>(0u, m_postProcessRC);
                list.SetUnorderedAccessView(1u, passData.finalTexture);
                list.SetBindlessHeap(2u);

                // Thread group size of 8 to reduce occupancy
                list.GetList()->Dispatch(
                    (passData.finalTexture->Width() + 7u) / 8u, (passData.finalTexture->Height() + 7u) / 8u, 1u);
                list.EndRender();

                passData.finalTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                renderer.compositeTexture = passData.finalTexture;
            });
    }
} // namespace Wild
