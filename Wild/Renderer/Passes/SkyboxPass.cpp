#include "Renderer/Passes/SkyboxPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"

namespace Wild
{
    SkyPass::SkyPass(std::string filePath)
    {
        m_cube = CreateCube();
        BufferDesc desc;
        m_cubeVertexBuffer = std::make_unique<Buffer>(desc);
        m_cubeVertexBuffer->CreateVertexBuffer<Vertex>(m_cube);

        BufferDesc bufferDesc{};
        bufferDesc.bufferSize = sizeof(CameraProjection);
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_cameraProjection[i] = std::make_unique<Buffer>(bufferDesc, BufferType::constant);
        }

        // Initial equirectangular texture map used as skybox
        m_skyboxTexture = std::make_unique<Texture>(filePath, TextureType::SKYBOX, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);

        // Posibly move these resources to the pass
        // Brdf look up table
        {
            TextureDesc texDesc;
            texDesc.width = 512;
            texDesc.Height = 512;
            texDesc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            texDesc.format = DXGI_FORMAT_R16G16_FLOAT;
            texDesc.name = "BRDF LUT";
            texDesc.shouldStayInCache = true;
            m_brdfLut = std::make_shared<Texture>(texDesc);
        }

        // Specular cube map resource
        {
            TextureDesc texDesc;
            texDesc.width = 512;
            texDesc.Height = 512;
            texDesc.Layers = 6;
            texDesc.type = TextureType::CUBEMAP;
            texDesc.mips = m_specularMips;
            texDesc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            texDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            texDesc.name = "Specular cube map";
            texDesc.shouldStayInCache = true;
            m_specularMap = std::make_shared<Texture>(texDesc);
        }
    }

    void SkyPass::Update(const float dt)
    {
        auto context = engine.GetGfxContext();
        auto ecs = engine.GetECS();
        auto& cameras = ecs->View<Camera>();

        CameraProjection camData{};

        // Loop over all camera's TODO make the code run for each camera entity
        for (auto& cameraEntity : cameras)
        {
            if (ecs->HasComponent<Camera>(cameraEntity))
            {
                auto& cam = ecs->GetComponent<Camera>(cameraEntity);

                camData.view = glm::mat4(glm::mat3(cam.GetView()));
                camData.proj = cam.GetProjection();
            }
        }

        int frameIndex = context->GetBackBufferIndex();
        m_cameraProjection[frameIndex]->Allocate(&camData);

        engine.GetImGui()->AddPanel("Debug skybox", [this]() {
            int skyMode = m_debugSkyboxMode;
            if (ImGui::SliderInt("Debug mode", &skyMode, 0, 6)) { m_debugSkyboxMode = skyMode; }
        });
    }

    void SkyPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        AddSkyboxPass(renderer, rg);
        AddIBLPass(renderer, rg);
    }

    /// <summary>
    /// Skybox pass is the last pass the reason for this is because the PBR pass draw to a full quad which overwrites the skybox
    /// with black pixels I didn't want my hdr skybox stored in a low color range so I just output it as the last pass.
    /// </summary>
    /// <param name="rg">Render Graph</param>
    void SkyPass::AddSkyboxPass(Renderer& renderer, RenderGraph& rg)
    {
        auto *passData = rg.AllocatePassData<SkyPassData>();
        auto *pbrData = rg.GetPassData<SkyPassData, PbrPassData>();

        passData->FinalTexture = pbrData->FinalTexture;
        passData->DepthTexture = pbrData->DepthTexture;

        rg.AddPass<SkyPassData>(
            "Skybox pass", PassType::Graphics, [&renderer, this](const SkyPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/SkyboxVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/SkyboxFrag.slang");
                settings.DepthStencilState.DepthEnable = true;
                settings.DepthStencilState.DepthWriteMask = DepthWriteMask::Zero;
                settings.DepthStencilState.DepthFunc = ComparisonFunc::LessEqual;
                settings.RasterizerState.CullMode = CullMode::None;
                // settings.RasterizerState.WindingMode = WindingOrder::Clockwise;

                // Setting up the input layout
                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

                // Uniforms
                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(SkyRootConstant)};
                uniforms.emplace_back(rootConstant);

                Uniform cameraBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(cameraBuffer);

                {
                    Uniform bindless{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE srvRange{};
                    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  UINT_MAX,
                                  0,
                                  0,
                                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                    // Second range for cube view
                    CD3DX12_DESCRIPTOR_RANGE srvRange1{};
                    srvRange1.Init(
                        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                    bindless.ranges.emplace_back(srvRange);
                    bindless.ranges.emplace_back(srvRange1);
                    bindless.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                    uniforms.emplace_back(bindless);
                }

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                staticSampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(staticSampler);

                auto& pipeline = renderer.GetOrCreatePipeline("Skybox pass", PipelineStateType::Graphics, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.FinalTexture},
                                 {ClearOperation::Store},
                                 passData.DepthTexture,
                                 DSClearOperation::Store,
                                 "Skybox pass");

                auto context = engine.GetGfxContext();
                int frameIndex = context->GetBackBufferIndex();

                SkyRootConstant rc{};
                // rc.view = m_skyboxTexture->GetSrv()->BindlessView();

                switch (m_debugSkyboxMode)
                {
                case 0:
                    rc.view = m_skyboxTexture->GetSrv()->BindlessView();
                    break;
                case 1:
                    if (renderer.irradianceMap) { rc.viewCube = renderer.irradianceMap->GetSrv()->BindlessView(); }
                    break;
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                    if (renderer.specularMap)
                    {
                        rc.viewCube = renderer.specularMap->GetSrv()->BindlessView();
                        rc.mipLevel = m_debugSkyboxMode - 2;
                    }
                    break;
                default:
                    break;
                }

                list.SetRootConstant<SkyRootConstant>(0, rc);

                list.SetConstantBufferView(1, m_cameraProjection[frameIndex].get());

                // m_skyboxTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                list.SetBindlessHeap(2);

                list.GetList()->IASetVertexBuffers(0, 1, &m_cubeVertexBuffer->GetVBView()->View());

                list.GetList()->DrawInstanced(36, 1, 0, 0);
                list.EndRender();

                renderer.compositeTexture = passData.FinalTexture;
            });
    }

    /// <summary>
    /// Generates all the data needed for Image Based Lighting like irradiance map, brdf lut and specular map.
    /// The pass doesn't cache the texture yet this still needs to be done.
    /// Resources are linked at the end of the pass and are currently only generated at the start of the pass.
    /// </summary>
    /// <param name="rg">Render Graph</param>
    void SkyPass::AddIBLPass(Renderer& renderer, RenderGraph& rg)
    {
        auto *passData = rg.AllocatePassData<IBLPassData>();

        {
            TextureDesc desc;
            desc.width = 512;
            desc.Height = 512;
            desc.Layers = 6;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "Environment cubemap";
            desc.usage = TextureDesc::gpuOnly;
            desc.type = TextureType::CUBEMAP;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->environmentCubeTexture = rg.CreateTransientTexture("Environtment cubemap", desc);
        }

        {
            TextureDesc desc;
            desc.width = 32;
            desc.Height = 32;
            desc.Layers = 6;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "Irradiance cubemap";
            desc.usage = TextureDesc::gpuOnly;
            desc.type = TextureType::CUBEMAP;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->irradianceTexture = rg.CreateTransientTexture("Irradiance cubemap", desc);
        }

        rg.AddPass<IBLPassData>(
            "IBL pass", PassType::Graphics, [&renderer, this](const IBLPassData& passData, CommandList& list) {
                // Matrices from https://learnopengl.com/PBR/IBL/Diffuse-irradiance
                glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
                glm::mat4 captureViews[] = {
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/ImageBasedLighting/CaptureIBLVert.slang");
                settings.ShaderState.FragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/ImageBasedLighting/CaptureIBLFrag.slang");
                settings.DepthStencilState.DepthEnable = false;
                settings.DepthStencilState.DepthWriteMask = DepthWriteMask::Zero;
                settings.DepthStencilState.DepthFunc = ComparisonFunc::LessEqual;
                settings.RasterizerState.CullMode = CullMode::None;

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R16G16B16A16_FLOAT);
                settings.depthFormat = DXGI_FORMAT_UNKNOWN;

                // Setting up the input layout for cube
                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

                /// Generating cube map from equirectangular map since it is easier to sample from and less expensive
                if (ShouldGenerateNewIBL)
                {
                    /// Convolute cubemap
                    {
                        // Size of the cubemap face
                        settings.RasterizerState.Viewport.size.x = 512;
                        settings.RasterizerState.Viewport.size.y = 512;

                        // Uniforms
                        std::vector<Uniform> uniforms;

                        Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(IBLRootConstant)};
                        uniforms.emplace_back(rootConstant);

                        Uniform bindless{0, 0, RootParams::RootResourceType::DescriptorTable};
                        CD3DX12_DESCRIPTOR_RANGE srvRange{};
                        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                      UINT_MAX,
                                      0,
                                      0,
                                      D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                        bindless.ranges.emplace_back(srvRange);
                        bindless.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                        uniforms.emplace_back(bindless);

                        Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                        staticSampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                        uniforms.emplace_back(staticSampler);

                        auto& pipeline =
                            renderer.GetOrCreatePipeline("IBL pass", PipelineStateType::Graphics, settings, uniforms);

                        for (size_t face = 0; face < 6; face++)
                        {
                            list.SetPipelineState(pipeline);
                            list.BeginRender({passData.environmentCubeTexture},
                                             {ClearOperation::Store},
                                             nullptr,
                                             DSClearOperation::Store,
                                             "IBL pass",
                                             face);

                            auto context = engine.GetGfxContext();
                            int frameIndex = context->GetBackBufferIndex();

                            IBLRootConstant rc{};
                            rc.projView = captureProjection * glm::mat4(glm::mat3(captureViews[face]));
                            rc.view = m_skyboxTexture->GetSrv()->BindlessView();

                            list.SetRootConstant<IBLRootConstant>(0, rc);

                            list.SetBindlessHeap(1);

                            list.GetList()->IASetVertexBuffers(0, 1, &m_cubeVertexBuffer->GetVBView()->View());

                            list.GetList()->DrawInstanced(36, 1, 0, 0);
                            list.EndRender();
                        }
                    }

                    /// Generate Irradiance map
                    {
                        settings.ShaderState.VertexShader =
                            engine.GetShaderTracker()->GetOrCreateShader("Shaders/ImageBasedLighting/CaptureIBLVert.slang");
                        settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader(
                            "Shaders/ImageBasedLighting/GenerateIrradianceMapFrag.slang");

                        // Size of the cubemap face
                        settings.RasterizerState.Viewport.size.x = 32;
                        settings.RasterizerState.Viewport.size.y = 32;

                        // Uniforms
                        std::vector<Uniform> uniforms;

                        Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(IBLRootConstant)};
                        uniforms.emplace_back(rootConstant);

                        // Uniform for setting up the bindless heap
                        Uniform bindless{0, 0, RootParams::RootResourceType::DescriptorTable};
                        CD3DX12_DESCRIPTOR_RANGE srvRange{};
                        srvRange.Init(
                            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                        bindless.ranges.emplace_back(srvRange);
                        bindless.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                        uniforms.emplace_back(bindless);

                        // Static sampler uniform
                        Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                        staticSampler.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                        staticSampler.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                        staticSampler.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                        uniforms.emplace_back(staticSampler);

                        auto& pipeline = renderer.GetOrCreatePipeline(
                            "Generate Irradiance map", PipelineStateType::Graphics, settings, uniforms);

                        /// Loop over all faces and render to each specific face via an index into the rtv
                        for (size_t face = 0; face < 6; face++)
                        {
                            list.SetPipelineState(pipeline);
                            list.BeginRender({passData.irradianceTexture},
                                             {ClearOperation::Store},
                                             nullptr,
                                             DSClearOperation::Store,
                                             "Generate Irradiance map",
                                             face);

                            auto context = engine.GetGfxContext();
                            int frameIndex = context->GetBackBufferIndex();

                            IBLRootConstant rc{};
                            rc.projView = captureProjection * glm::mat4(glm::mat3(captureViews[face]));

                            passData.environmentCubeTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                            rc.view = passData.environmentCubeTexture->GetSrv()->BindlessView();

                            list.SetRootConstant<IBLRootConstant>(0, rc);
                            list.SetBindlessHeap(1);

                            list.GetList()->IASetVertexBuffers(0, 1, &m_cubeVertexBuffer->GetVBView()->View());

                            list.GetList()->DrawInstanced(36, 1, 0, 0);
                            list.EndRender();
                        }

                        passData.irradianceTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    }

                    /// BRDF look up table pass generated with compute
                    {
                        PipelineStateSettings computeSettings{};
                        computeSettings.ShaderState.ComputeShader =
                            engine.GetShaderTracker()->GetOrCreateShader("Shaders/ImageBasedLighting/GenerateBrdfLut.slang");

                        std::vector<Uniform> uniforms;

                        // Uniform for setting up the bindless heap
                        Uniform brdfLutUAVTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                        CD3DX12_DESCRIPTOR_RANGE uavRange{};
                        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                        brdfLutUAVTexture.ranges.emplace_back(uavRange);
                        uniforms.emplace_back(brdfLutUAVTexture);

                        auto& pipeline = renderer.GetOrCreatePipeline(
                            "Generate brdf lut pass", PipelineStateType::Compute, computeSettings, uniforms);

                        list.SetPipelineState(pipeline);
                        list.BeginRender();

                        list.SetUnorderedAccessView(0, m_brdfLut.get());

                        list.GetList()->Dispatch((m_brdfLut->Width() + 7) / 8, (m_brdfLut->Height() + 7) / 8, 1);
                        list.EndRender();

                        m_brdfLut->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    }

                    /// Specular reflection pass generated with compute
                    {
                        PipelineStateSettings computeSettings{};
                        computeSettings.ShaderState.ComputeShader =
                            engine.GetShaderTracker()->GetOrCreateShader("Shaders/ImageBasedLighting/GenerateSpecularMap.slang");

                        std::vector<Uniform> uniforms;

                        Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(SpecularMapRootConstant)};
                        uniforms.emplace_back(rootConstant);

                        // UAV uniform for specular cube map
                        Uniform specularCubemapTexture{0, 0, RootParams::RootResourceType::DescriptorTable};
                        CD3DX12_DESCRIPTOR_RANGE uavRange{};
                        uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                        specularCubemapTexture.ranges.emplace_back(uavRange);
                        uniforms.emplace_back(specularCubemapTexture);

                        // Uniform for setting up the bindless heap cubemap textures
                        Uniform cubeMapTextures{0, 0, RootParams::RootResourceType::DescriptorTable};
                        CD3DX12_DESCRIPTOR_RANGE srvRange{};
                        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                      UINT_MAX,
                                      0,
                                      0,
                                      D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindless
                        cubeMapTextures.ranges.emplace_back(srvRange);
                        uniforms.emplace_back(cubeMapTextures);

                        Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                        staticSampler.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                        staticSampler.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                        uniforms.emplace_back(staticSampler);

                        auto& pipeline = renderer.GetOrCreatePipeline(
                            "Specular reflection pass", PipelineStateType::Compute, computeSettings, uniforms);
                        list.SetPipelineState(pipeline);
                        list.BeginRender();

                        SpecularMapRootConstant rc{};

                        if (passData.environmentCubeTexture)
                        {
                            rc.environmentView = passData.environmentCubeTexture->GetSrv()->BindlessView();
                            passData.environmentCubeTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                        }

                        /// Loop over all possible faces and mips and generate the UAV resource for their respective index into
                        /// the heap
                        auto desc = m_specularMap->GetDesc();
                        for (uint32_t mip = 0; mip < desc.mips; mip++)
                        {
                            for (uint32_t face = 0; face < desc.Layers; face++)
                            {
                                uint32_t uavIndex = mip * desc.Layers + face;
                                rc.mipSize = desc.width * std::pow(0.5, mip);
                                rc.roughness = (float)mip / (float)(desc.mips - 1);
                                rc.face = face;

                                list.SetRootConstant(0, rc);
                                list.SetUnorderedAccessView(1, m_specularMap.get(), uavIndex);
                                list.SetBindlessHeap(2);

                                list.GetList()->Dispatch((rc.mipSize + 31) / 32, (rc.mipSize + 31) / 32, 1);
                            }
                        }

                        list.EndRender();

                        // Transition resources so they are ready for use
                        m_brdfLut->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                        m_specularMap->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                        passData.environmentCubeTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    }

                    // Set all their resources into the renderer TODO modify so that they are set in the pass
                    renderer.irradianceMap = passData.irradianceTexture;
                    renderer.specularMap = m_specularMap.get();
                    renderer.brdfLut = m_brdfLut.get();
                    ShouldGenerateNewIBL = false;
                }
            });
    }

    std::vector<Vertex> Wild::SkyPass::CreateCube()
    {
        return {// Front face
                {{-0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0, 1}, {0, 0}},
                {{0.5f, -0.5f, 0.5f}, {0, 1, 0}, {0, 0, 1}, {1, 0}},
                {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0, 1}, {1, 1}},
                {{-0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0, 1}, {0, 0}},
                {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0, 1}, {1, 1}},
                {{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 0, 1}, {0, 1}},

                // Back face
                {{0.5f, -0.5f, -0.5f}, {1, 1, 0}, {0, 0, -1}, {0, 0}},
                {{-0.5f, -0.5f, -0.5f}, {0, 1, 1}, {0, 0, -1}, {1, 0}},
                {{-0.5f, 0.5f, -0.5f}, {1, 0, 1}, {0, 0, -1}, {1, 1}},
                {{0.5f, -0.5f, -0.5f}, {1, 1, 0}, {0, 0, -1}, {0, 0}},
                {{-0.5f, 0.5f, -0.5f}, {1, 0, 1}, {0, 0, -1}, {1, 1}},
                {{0.5f, 0.5f, -0.5f}, {0, 1, 1}, {0, 0, -1}, {0, 1}},

                // Left face
                {{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {-1, 0, 0}, {0, 0}},
                {{-0.5f, -0.5f, 0.5f}, {0, 1, 0}, {-1, 0, 0}, {1, 0}},
                {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {-1, 0, 0}, {1, 1}},
                {{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {-1, 0, 0}, {0, 0}},
                {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {-1, 0, 0}, {1, 1}},
                {{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {-1, 0, 0}, {0, 1}},

                // Right face
                {{0.5f, -0.5f, 0.5f}, {1, 1, 0}, {1, 0, 0}, {0, 0}},
                {{0.5f, -0.5f, -0.5f}, {1, 0, 1}, {1, 0, 0}, {1, 0}},
                {{0.5f, 0.5f, -0.5f}, {0, 1, 1}, {1, 0, 0}, {1, 1}},
                {{0.5f, -0.5f, 0.5f}, {1, 1, 0}, {1, 0, 0}, {0, 0}},
                {{0.5f, 0.5f, -0.5f}, {0, 1, 1}, {1, 0, 0}, {1, 1}},
                {{0.5f, 0.5f, 0.5f}, {1, 0, 1}, {1, 0, 0}, {0, 1}},

                // Top face
                {{-0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 0}},
                {{0.5f, 0.5f, -0.5f}, {0, 1, 1}, {0, 1, 0}, {1, 0}},
                {{0.5f, 0.5f, 0.5f}, {1, 0, 1}, {0, 1, 0}, {1, 1}},
                {{-0.5f, 0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 0}},
                {{0.5f, 0.5f, 0.5f}, {1, 0, 1}, {0, 1, 0}, {1, 1}},
                {{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 1, 0}, {0, 1}},

                // Bottom face
                {{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, -1, 0}, {0, 0}},
                {{0.5f, -0.5f, -0.5f}, {0, 0, 1}, {0, -1, 0}, {1, 0}},
                {{0.5f, -0.5f, 0.5f}, {0, 1, 0}, {0, -1, 0}, {1, 1}},
                {{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, -1, 0}, {0, 0}},
                {{0.5f, -0.5f, 0.5f}, {0, 1, 0}, {0, -1, 0}, {1, 1}},
                {{-0.5f, -0.5f, 0.5f}, {1, 1, 0}, {0, -1, 0}, {0, 1}}};
    }
} // namespace Wild
