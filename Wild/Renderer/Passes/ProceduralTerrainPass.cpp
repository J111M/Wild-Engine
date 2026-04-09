#include "Renderer/Passes/ProceduralTerrainPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"

namespace Wild
{
    ProceduralTerrainPass::ProceduralTerrainPass()
    {
        GenerateTerrainPlane(64);

        m_drc.waterLevel = m_grc.waterLevel;

        // Create buffer for all terrain textures
        BufferDesc terrainTextureBuffer{};
        terrainTextureBuffer.bufferSize = sizeof(TerrainTextures);
        m_terrainTexturesCbv = std::make_unique<Buffer>(terrainTextureBuffer, BufferType::constant);

        BufferDesc cameraBufferDesc{};
        cameraBufferDesc.bufferSize = sizeof(ProjViewCamera);
        m_cameraCbv = std::make_unique<Buffer>(cameraBufferDesc, BufferType::constant);

        // Load all terrain textures
        grassTexture = std::make_unique<Texture>("Assets/Textures/grass/Grass008_2K-JPG_Color.jpg");
        grassNormalTexture = std::make_unique<Texture>("Assets/Textures/grass/Grass008_2K-JPG_NormalDX.jpg");
        grassRoughTexture = std::make_unique<Texture>("Assets/Textures/grass/Grass008_2K-JPG_Roughness.jpg");
        grassAOTexture = std::make_unique<Texture>("Assets/Textures/grass/Grass008_2K-JPG_AmbientOcclusion.jpg");

        sandTexture = std::make_unique<Texture>("Assets/Textures/sand/Ground054_2K-JPG_Color.jpg");
        sandNormalTexture = std::make_unique<Texture>("Assets/Textures/sand/Ground054_2K-JPG_NormalDX.jpg");
        sandRoughTexture = std::make_unique<Texture>("Assets/Textures/sand/Ground054_2K-JPG_Roughness.jpg");
        sandAOTexture = std::make_unique<Texture>("Assets/Textures/sand/Ground054_2K-JPG_AmbientOcclusion.jpg");

        rockTexture = std::make_unique<Texture>("Assets/Textures/rock/Rock061_2K-JPG_Color.jpg");
        rockNormalTexture = std::make_unique<Texture>("Assets/Textures/rock/Rock061_2K-JPG_NormalDX.jpg");
        rockRoughTexture = std::make_unique<Texture>("Assets/Textures/rock/Rock061_2K-JPG_Roughness.jpg");
        rockAOTexture = std::make_unique<Texture>("Assets/Textures/rock/Rock061_2K-JPG_AmbientOcclusion.jpg");

        noiseBlendTexture = std::make_unique<Texture>("Assets/Textures/noiseBlend.png");

        // Grass textures
        m_terrainTexturesView.grassTexture = grassTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.grassNormalTexture = grassNormalTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.grassRoughTexture = grassRoughTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.grassAOTexture = grassAOTexture->GetSrv()->BindlessView();

        // Sand texture
        m_terrainTexturesView.sandTexture = sandTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.sandNormalTexture = sandNormalTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.sandRoughTexture = sandRoughTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.sandAOTexture = sandAOTexture->GetSrv()->BindlessView();

        // Rock texture
        m_terrainTexturesView.rockTexture = rockTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.rockNormalTexture = rockNormalTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.rockRoughTexture = rockRoughTexture->GetSrv()->BindlessView();
        m_terrainTexturesView.rockAOTexture = rockAOTexture->GetSrv()->BindlessView();

        m_terrainTexturesView.noiseBlendTexture = noiseBlendTexture->GetSrv()->BindlessView();

        m_terrainTexturesCbv->Allocate(&m_terrainTexturesView);
    }

    void ProceduralTerrainPass::Update(const float dt)
    {
        engine.GetImGui()->AddPanel("Terrain Settings", [this]() {
            ImGui::SliderFloat("Amplitude", &m_grc.amplitude, 0.01f, 1.0f);
            ImGui::SliderFloat("Frequency", &m_grc.frequency, 0.001f, 0.1f);
            ImGui::SliderFloat("Amplitude Scalar", &m_grc.amplitudeScalar, 0.01f, 1.0f);
            ImGui::SliderFloat("Frequency Scalar", &m_grc.frequencyScalar, 0.1f, 4.0f);
            ImGui::SliderInt("Octaves", &m_grc.octaves, 1, 12);

            ImGui::SliderFloat("Noise blend: ", &m_drc.noiseBlend, 0.0f, 1.0f);

            ImGui::SliderInt("Seed: ", reinterpret_cast<int*>(&m_grc.seed), 0, 10000);

            if (ImGui::CollapsingHeader("Advanced Settings"))
            {
                ImGui::SliderFloat("Warp Strength", &m_grc.warpStrength, 0.0f, 2.0f);
                ImGui::SliderFloat("Elevation Power", &m_grc.elevationPower, -4.0f, 6.0f);
                ImGui::SliderFloat("Ridge Blend", &m_grc.ridgeBlend, 0.0f, 4.0f);
                ImGui::SliderFloat("Valley Strength", &m_grc.valleyScalar, -2.0f, 20.0f);
                ImGui::SliderFloat("Water Level", &m_grc.waterLevel, -0.6f, 0.6f);
                ImGui::SliderFloat("Warp scalar", &m_grc.warpScalar, 0.0f, 2.5f);
                ImGui::SliderFloat("Fallof scalar", &m_grc.fallOffScalar, 0.0f, 2.5f);
                m_drc.waterLevel = m_grc.waterLevel;
            }

            if (ImGui::Button("Generate new terrain")) { m_shouldGenerateChunks = true; }

            ImGui::Checkbox("Repeat generation", &m_keepGeneratingTerrain);

            if (m_keepGeneratingTerrain) { m_shouldGenerateChunks = true; }
        });
    }

    void ProceduralTerrainPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        GenerateTerrainPass(renderer, rg);
        DrawTerrainPass(renderer, rg);
    }

    void ProceduralTerrainPass::GenerateTerrainPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<GenerateTerrainPassData>();

        rg.AddPass<GenerateTerrainPassData>(
            "Procedural terrain pass",
            PassType::Compute,
            [&renderer, this](GenerateTerrainPassData& passData, CommandList& list) {
                if (m_shouldGenerateChunks)
                {
                    const uint32_t chunkWidth = 1u;
                    const uint32_t chunkHeight = 1u;

                    const uint32_t texWidth = 512;
                    const uint32_t texHeight = 512;

                    if (tempChunkID == 0)
                    {
                        TextureDesc desc{};
                        desc.width = texWidth;
                        desc.height = texHeight;
                        desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
                        desc.format = DXGI_FORMAT_R32_FLOAT;

                        // Chunk size
                        for (uint32_t y = 0; y < chunkHeight; y++)
                        {
                            for (uint32_t x = 0; x < chunkWidth; x++)
                            {
                                Entity chunkEntity = engine.GetECS()->CreateEntity();
                                auto& transform =
                                    engine.GetECS()->AddComponent<Transform>(chunkEntity, glm::vec3(0, 0, 0), m_chunkEntity);
                                transform.SetPosition(glm::vec3(x * 128, 0, y * 128));

                                auto& chunk = engine.GetECS()->AddComponent<TerrainChunk>(chunkEntity);
                                desc.name = "Chunk number X: " + std::to_string(x) + " | Y: " + std::to_string(y);
                                chunk.heightMap = std::make_unique<Texture>(desc);
                                chunk.id = y * chunkWidth + x;

                                m_terrainChunks.emplace_back(chunkEntity);
                            }
                        }

                        tempChunkID++;
                    }

                    PipelineStateSettings computeSettings{};
                    computeSettings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/ProceduralTerrainCompute.slang");

                    std::vector<Uniform> uniforms;

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(GenerateTerrainRootConstant)};
                    uniforms.emplace_back(rootConstant);

                    // UAV uniform for specular cube map
                    Uniform heightMap{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    heightMap.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(heightMap);

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Procedural terrain pass", PipelineStateType::Compute, computeSettings, uniforms);

                    for (size_t i = 0; i < m_terrainChunks.size(); i++)
                    {
                        list.SetPipelineState(pipeline);
                        list.BeginRender();
                        m_grc.textureSize = glm::vec2(texWidth, texHeight);

                        size_t x = i % chunkWidth;
                        size_t y = i / chunkWidth;
                        m_grc.chunkPosition = glm::vec2(x * (texWidth - 1), y * (texHeight - 1));

                        auto& chunk = engine.GetECS()->GetComponent<TerrainChunk>(m_terrainChunks[i]);

                        list.SetRootConstant(0, m_grc);
                        list.SetUnorderedAccessView(1, chunk.heightMap.get());
                        list.GetList()->Dispatch((texWidth) / 32, (texHeight) / 32, 1);

                        list.EndRender();

                        chunk.heightMap->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    }

                    m_shouldGenerateChunks = false;
                }
            });
    }

    void ProceduralTerrainPass::DrawTerrainPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DrawTerrainPassData>();
        auto* heightMapData = rg.GetPassData<DrawTerrainPassData, GenerateTerrainPassData>();

        // Albedo
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.name = "Albedo render target";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            passData->albedoRoughnessTexture = rg.CreateTransientTexture("AlbedoRoughnessTexture", desc);
        }

        // Normals
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.name = "Normal render target";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            passData->normalMetallicTexture = rg.CreateTransientTexture("NormalMetallicTexture", desc);
        }

        // emissive
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.name = "Emissive render target";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
            passData->emissiveTexture = rg.CreateTransientTexture("EmissiveTexture", desc);
        }

        // DepthStencil
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            desc.name = "DepthStencil Texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(
                TextureDesc::depthStencil | TextureDesc::shaderResource); // Automatically uses depth stencil format
            passData->depthTexture = rg.CreateTransientTexture("DepthStencil", desc);
        }

        rg.AddPass<DrawTerrainPassData>(
            "Draw procedural terrain pass",
            PassType::Graphics,
            [&renderer, heightMapData, this](DrawTerrainPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DrawTerrainVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DrawTerrainFrag.slang");
                settings.DepthStencilState.DepthEnable = true;

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Albedo
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Normal
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Emissive

                // Setting up the input layout
                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DrawTerrainRootConstant)};
                uniforms.emplace_back(rootConstant);

                Uniform TextureViewBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(TextureViewBuffer);

                Uniform projViewBuffer{2, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(projViewBuffer);

                Uniform bindlessUni{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srvRange{};
                srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                bindlessUni.ranges.emplace_back(srvRange);

                uniforms.emplace_back(bindlessUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                staticSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                staticSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(staticSampler);

                Uniform staticAnisotrophicSampler{1, 0, RootParams::RootResourceType::StaticSampler};
                staticAnisotrophicSampler.samplerState.filter = D3D12_FILTER_ANISOTROPIC;
                staticAnisotrophicSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                uniforms.emplace_back(staticAnisotrophicSampler);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Draw terrain chunks pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                auto ecs = engine.GetECS();

                Camera* camera = GetActiveCamera();

                if (camera)
                {
                    m_pvc.m_viewProj = camera->GetProjection() * camera->GetView();
                    m_cameraCbv->Allocate(&m_pvc);
                }

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Clear, ClearOperation::Clear, ClearOperation::Clear},
                                 passData.depthTexture,
                                 DSClearOperation::DepthClear,
                                 "PCG pass");

                for (auto [entity, chunk, transform] : ecs->GetRegistry().view<TerrainChunk, Transform>().each())
                {
                    if (camera)
                    {
                        m_drc.worldMatix = transform.GetWorldMatrix();
                        m_drc.invModel = glm::transpose(glm::inverse(glm::mat3(transform.GetWorldMatrix())));
                    }

                    m_drc.heightMapView = chunk.heightMap->GetSrv()->BindlessView();

                    list.SetRootConstant<DrawTerrainRootConstant>(0, m_drc);
                    list.SetConstantBufferView(1, m_terrainTexturesCbv.get());
                    list.SetConstantBufferView(2, m_cameraCbv.get());
                    list.SetBindlessHeap(3);

                    list.GetList()->IASetVertexBuffers(0, 1, &m_terrainVertices->GetVBView()->View());
                    list.GetList()->IASetIndexBuffer(&m_terrainIndices->GetIBView()->View());
                    list.GetList()->DrawIndexedInstanced(m_drawCount, 1, 0, 0, 0);
                }

                list.EndRender();
            });
    }

    void ProceduralTerrainPass::GenerateTerrainPlane(uint32_t resolution)
    {
        float size = 128.0f;
        float halfSize = size / 2.0f;
        float step = size / resolution;

        std::vector<Vertex> vertices;
        vertices.reserve(static_cast<size_t>(resolution) * static_cast<size_t>(resolution));

        // Generate vertices
        for (uint32_t z = 0; z <= resolution; z++)
        {
            for (uint32_t x = 0; x <= resolution; x++)
            {
                Vertex vertex;
                vertex.position = {-halfSize + x * step, 0.0f, -halfSize + z * step};
                vertex.color = {1.0f, 1.0f, 1.0f};
                vertex.normal = {0.0f, 1.0f, 0.0f};
                vertex.uv = {(float)x / resolution, (float)z / resolution};
                vertices.push_back(vertex);
            }
        }

        BufferDesc vDesc{};
        m_terrainVertices = std::make_unique<Buffer>(vDesc);
        m_terrainVertices->CreateVertexBuffer(vertices);

        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(resolution) * static_cast<size_t>(resolution) * 6);

        // Generate indices
        for (uint32_t z = 0; z < resolution; z++)
        {
            for (uint32_t x = 0; x < resolution; x++)
            {
                uint32_t topLeft = z * (resolution + 1) + x;
                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = (z + 1) * (resolution + 1) + x;
                uint32_t bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        BufferDesc iDesc{};
        m_terrainIndices = std::make_unique<Buffer>(vDesc);
        m_terrainIndices->CreateIndexBuffer(indices);

        m_drawCount = indices.size();
    };
} // namespace Wild
