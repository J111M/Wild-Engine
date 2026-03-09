#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

namespace Wild
{
    PbrPass::PbrPass()
    {
        auto ecs = engine.GetECS();
        for (size_t y = 1; y < 2; y++)
        {
            for (size_t x = 1; x < 2; x++)
            {
                auto entity = ecs->CreateEntity();
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                auto& light = ecs->AddComponent<PointLight>(entity);

                transform.Name = std::string("Light: " + std::to_string(x + y));

                transform.SetPosition(glm::vec3(x * 5, 15, y * 5));
                light.position = transform.GetPosition();

                float intensity = ((x + y) / 2.0f) + 1.0f;
                light.colorIntensity = glm::vec4(glm::vec3(1.0, 0.0, 0.0), 20.0f);
            }
        }

        // Create point light buffer
        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(PointLight) * MAX_POINT_LIGHTS;
            m_pointLightsBuffer = std::make_unique<Buffer>(desc, BufferType::constant);
        }

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(CameraBuffer);
            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                m_cameraBuffer[i] = std::make_unique<Buffer>(desc, BufferType::constant);
            }
        }

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(PBRData);
            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                m_pbrDataBuffer[i] = std::make_unique<Buffer>(desc, BufferType::constant);
            }
        }

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(EnvironmentData);
            m_environmentData = std::make_unique<Buffer>(desc, BufferType::constant);
        }
    }

    void PbrPass::Update(const float dt)
    {
        auto context = engine.GetGfxContext();
        auto ecs = engine.GetECS();

        std::vector<PointLight> lightData;
        auto& lightsView = ecs->GetRegistry().view<PointLight, Transform>();

        for (auto [entity, pointLight, transform] : lightsView.each())
        {
            // auto& light = lightsView.get<PointLight>(entity);
            PointLight gpuLight{};
            gpuLight.position = transform.GetPosition();
            gpuLight.colorIntensity = pointLight.colorIntensity;
            lightData.push_back(gpuLight);
        }

        m_pbrData.numOfPointLights = lightData.size();
        m_pointLightsBuffer->Allocate(lightData.data());

        Camera* camera = GetActiveCamera();

        if (camera)
        {
            m_camData.inverseView = glm::inverse(camera->GetView());
            m_camData.inverseProj = glm::inverse(camera->GetProjection());
            m_camData.viewSpace = camera->GetProjection() * camera->GetView();
            m_camData.cameraFar = camera->GetNearFar().y;
            m_pbrData.cameraPosition = camera->GetPosition();
        }

        int frameIndex = context->GetBackBufferIndex();
        m_cameraBuffer[frameIndex]->Allocate(&m_camData);

        engine.GetImGui()->AddPanel("Pbr settings", [this]() {
            ImGui::SliderFloat3("Light direction: ", &m_pbrData.lightDirection[0], -20.0f, 20.0f);

            const char* debugModes[] = {"None", "Albedo", "Normals", "Roughness", "Metallic", "AO", "Depth"};

            ImGui::Combo("Debug view Mode", (int*)&m_pbrData.viewMode, debugModes, IM_ARRAYSIZE(debugModes));
        });

        m_pbrDataBuffer[frameIndex]->Allocate(&m_pbrData);
    }

    void PbrPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<PbrPassData>();
        auto* deferredData = rg.GetPassData<PbrPassData, DeferredPassData>();
        auto* shadowMapData = rg.GetPassData<PbrPassData, CsmPassData>();

        passData->pointlights = m_pointLightsBuffer;
        passData->numOfPointLights = m_pbrData.numOfPointLights;
        passData->depthTexture = deferredData->depthTexture;

        // Final texture
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "FinalPbrTexture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            passData->finalTexture = rg.CreateTransientTexture("FinalPbrTexture", desc);
        }

        rg.AddPass<PbrPassData>(
            "Pbr assembly pass",
            PassType::Graphics,
            [&renderer, deferredData, shadowMapData, this](const PbrPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/PbrVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/PbrFrag.slang");
                settings.DepthStencilState.DepthEnable = false;
                settings.RasterizerState.WindingMode = WindingOrder::Clockwise;
                settings.renderTargetsFormat.push_back(passData.finalTexture->GetDesc().format);

                std::vector<Uniform> uniforms;

                Uniform rootConstantUni{0, 0, RootParams::RootResourceType::Constants, sizeof(PbrRootConstant)};
                rootConstantUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(rootConstantUni);

                // Inverse camera buffer
                Uniform invCameraUni{1, 0, RootParams::RootResourceType::ConstantBufferView};
                invCameraUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(invCameraUni);

                Uniform pbrUni{2, 0, RootParams::RootResourceType::ConstantBufferView};
                pbrUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(pbrUni);

                Uniform envUni{3, 0, RootParams::RootResourceType::ConstantBufferView};
                envUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(envUni);

                Uniform pointLightUni{4, 0, RootParams::RootResourceType::ConstantBufferView};
                pointLightUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(pointLightUni);

                Uniform shadowMapUni{5, 0, RootParams::RootResourceType::ConstantBufferView};
                shadowMapUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(shadowMapUni);

                // Bindless resources
                Uniform BindlessHeap{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srvRange{};
                srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                CD3DX12_DESCRIPTOR_RANGE srvRange1{};
                srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                               UINT_MAX,
                               0,
                               1,
                               D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for cube maps
                BindlessHeap.ranges.emplace_back(srvRange);
                BindlessHeap.ranges.emplace_back(srvRange1);
                BindlessHeap.visibility = D3D12_SHADER_VISIBILITY_PIXEL;

                uniforms.emplace_back(BindlessHeap);

                Uniform sampler{0, 0, RootParams::RootResourceType::StaticSampler};
                sampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                sampler.samplerState.filter =
                    D3D12_FILTER_MIN_MAG_MIP_LINEAR; // D3D12_FILTER_MIN_MAG_MIP_LINEAR; //D3D12_FILTER_MIN_MAG_MIP_LINEAR
                sampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(sampler);

                Uniform shadowSampler{1, 0, RootParams::RootResourceType::StaticSampler};
                shadowSampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                shadowSampler.samplerState.filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
                shadowSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                shadowSampler.samplerState.comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
                // shadowSampler.samplerState.borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
                uniforms.emplace_back(shadowSampler);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Pbr assembly pass", PipelineStateType::Graphics, settings, uniforms);

                EnvironmentData envData{};
                if (renderer.irradianceMap)
                {
                    envData.irradianceView = renderer.irradianceMap->GetSrv()->BindlessView();
                    envData.specularView = renderer.specularMap->GetSrv()->BindlessView();
                    envData.brdfView = renderer.brdfLut->GetSrv()->BindlessView();
                }

                m_environmentData->Allocate(&envData);

                list.SetPipelineState(pipeline);
                list.BeginRender(
                    {passData.finalTexture}, {ClearOperation::Store}, nullptr, DSClearOperation::Store, "Pbr assembly pass");

                deferredData->albedoRoughnessTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                deferredData->normalMetallicTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                deferredData->emissiveTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                deferredData->depthTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                m_rc.albedoView = deferredData->albedoRoughnessTexture->GetSrv()->BindlessView();
                m_rc.normalView = deferredData->normalMetallicTexture->GetSrv()->BindlessView();
                m_rc.emissiveView = deferredData->emissiveTexture->GetSrv()->BindlessView();
                m_rc.depthView = deferredData->depthTexture->GetSrv()->BindlessView();
                m_rc.depthBias = shadowMapData->biasValue;

                // Get shader resource view's from the shadowmap depth textures
                for (size_t cascade = 0; cascade < SHADOWMAP_CASCADES; cascade++)
                {
                    m_rc.shadowMapView[cascade] = shadowMapData->shadowMap[cascade]->GetSrv()->BindlessView();
                }

                list.SetRootConstant<PbrRootConstant>(0, m_rc);

                auto context = engine.GetGfxContext();
                int frameIndex = context->GetBackBufferIndex();

                list.SetConstantBufferView(1, m_cameraBuffer[frameIndex].get());
                list.SetConstantBufferView(2, m_pbrDataBuffer[frameIndex].get());
                list.SetConstantBufferView(3, m_environmentData.get());
                list.SetConstantBufferView(4, m_pointLightsBuffer.get());
                list.SetConstantBufferView(5, shadowMapData->directLightBuffer.get());
                list.SetBindlessHeap(6);

                list.GetList()->DrawInstanced(3, 1, 0, 0);
                list.EndRender();
            });
    }
} // namespace Wild
