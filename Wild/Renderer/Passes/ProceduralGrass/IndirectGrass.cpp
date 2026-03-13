#include "Renderer/Passes/ProceduralGrass/IndirectGrass.hpp"
#include "Renderer/Passes/ProceduralTerrainPass.hpp"

#include <glm/gtc/matrix_access.hpp>

namespace Wild
{
    IndirectGrass::IndirectGrass()
    {
        BufferDesc desc{};
        desc.bufferSize = sizeof(GrassBladeData);
        desc.numOfElements = MAXGRASSBLADES;
        m_perBladeDataBuffer = std::make_unique<Buffer>(desc, BufferType::uav);

        // Generate grass mesh data
        CreateGrassMeshes();

        // Cull data UAV, u0
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            BufferDesc desc{};
            desc.name = "Cullinstance buffer: " + std::to_string(i);
            desc.bufferSize = sizeof(CulledInstance);
            desc.numOfElements = MAXGRASSBLADES;
            m_culledInstancesBuffer[i] = std::make_shared<Buffer>(desc, BufferType::uav);
        }

        // Single uint for writing instance count UAV, u1
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            BufferDesc desc{};
            desc.name = "Indirect instance count buffer: " + std::to_string(i);
            desc.bufferSize = sizeof(uint32_t);
            desc.numOfElements = m_lodAmount;
            m_instanceCountBuffer[i] = std::make_unique<Buffer>(desc, BufferType::uav);
        }

        // Frustum constant buffer
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(FrustumBuffer);
            m_frustumBuffer[i] = std::make_unique<Buffer>(desc, BufferType::constant);
        }

        // We need to create the commands on the gpu we do that in this buffer
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            BufferDesc desc{};
            desc.name = "Indirect commands buffer: " + std::to_string(i);
            desc.bufferSize = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
            desc.numOfElements = m_lodAmount;
            m_drawCommandsBuffer[i] = std::make_unique<Buffer>(desc, BufferType::uav);
        }

        D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};
        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &argumentDesc;

        auto device = engine.GetGfxContext()->GetDevice();
        device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&m_commandSignature));

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(SceneData);

            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                m_sceneData[i] = std::make_shared<Buffer>(desc, BufferType::constant);

                // Keep buffer data mapped for cpu write access
                CD3DX12_RANGE readRange(0, 0);
                m_sceneData[i]->Map(&readRange);
            }
        }

        m_chunkEntity = engine.GetECS()->CreateEntity();
        auto& transform = engine.GetECS()->AddComponent<Transform>(m_chunkEntity, glm::vec3(0, 0, 0), m_chunkEntity);
        transform.SetPosition(glm::vec3(-64, 0, -64));
    }

    void IndirectGrass::Add(Renderer& renderer, RenderGraph& rg)
    {
        // Adds each pass to the render graph
        AddComputePerBladeDataPass(renderer, rg);
        AddClearCounterPass(renderer, rg);
        AddGrassCulling(renderer, rg);
        AddIndirectDrawCommandsPass(renderer, rg);
        AddRenderGrass(renderer, rg);
    }

    void IndirectGrass::Update(const float dt)
    {
        auto& device = engine.GetGfxContext();
        auto& ecs = engine.GetECS();

        engine.GetImGui()->AddPanel("Grass Settings", [this]() {
            ImGui::SliderFloat("Wind Strength", &m_grassSceneData.windStrength, 0.01f, 20.0f);
            ImGui::SliderFloat("Octaves", &m_grassSceneData.octaves, 0.1, 1);
            ImGui::SliderFloat("Frequency", &m_grassSceneData.frequency, 0.01f, 0.4f);
            ImGui::SliderFloat("Amplitude", &m_grassSceneData.amplitude, 0.01f, 1.0f);
            ImGui::SliderFloat2("Wind Direction", &m_grassSceneData.windDirection.x, -10.0f, 10.0f);

            if (ImGui::Button("Recompute blades")) { m_recomputeGrassBlades = true; }
        });

        SceneData SceneCbv{};

        Camera* cam = GetActiveCamera();

        if (cam)
        {
            SceneCbv.ProjView = cam->GetProjection() * cam->GetView();
            SceneCbv.CameraPosition = cam->GetPosition();

            SceneCbv.windStrength = m_grassSceneData.windStrength;
            SceneCbv.octaves = m_grassSceneData.octaves;
            SceneCbv.frequency = m_grassSceneData.frequency;
            SceneCbv.amplitude = m_grassSceneData.amplitude;
            SceneCbv.windDirection = m_grassSceneData.windDirection;
        }

        m_sceneData[device->GetBackBufferIndex()]->Allocate(&SceneCbv);
        UpdateFrustumData(static_cast<int>(device->GetBackBufferIndex()));

        m_accumulatedTime += dt * 1;
        m_rc.time = m_accumulatedTime;
    }

    /// <summary>
    /// Pass that computes my per blade grass data the data will be overwritten when a chunk changes
    /// </summary>
    void IndirectGrass::AddComputePerBladeDataPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<PerBladePassData>();
        auto* chunkDepency = rg.GetPassData<PerBladePassData, GenerateTerrainPassData>();

        rg.AddPass<PerBladePassData>(
            "Compute per blade data pass", PassType::Compute, [&renderer, this](PerBladePassData& countData, CommandList& list) {
                if (m_recomputeGrassBlades)
                {
                    PipelineStateSettings settings{};
                    settings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputePerGrassBladeData.slang");

                    std::vector<Uniform> uniforms;
                    Uniform perBladeBuffer{
                        0, 0, RootParams::RootResourceType::UnorderedAccessView, sizeof(GrassBladeData) * MAXGRASSBLADES};
                    uniforms.emplace_back(perBladeBuffer);

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(PerBladeComputeRootConstant)};
                    uniforms.emplace_back(rootConstant);

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

                    m_perBladeDataBuffer->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Compute per blade data pass", PipelineStateType::Compute, settings, uniforms);
                    list.SetPipelineState(pipeline);
                    list.BeginRender("Compute per blade data pass");

                    for (auto [entity, chunk, transform] : engine.GetECS()->GetRegistry().view<TerrainChunk, Transform>().each())
                    {
                        auto context = engine.GetGfxContext();
                        UINT frameIndex = context->GetBackBufferIndex();

                        m_pbcrc.modelMatrix = glm::mat4{1.0f}; // TODO implement later for frustum culling
                        m_pbcrc.chunkPosition.x = transform.GetPosition().x;
                        m_pbcrc.chunkPosition.y = transform.GetPosition().z;
                        m_pbcrc.chunkId = chunk.id;
                        m_pbcrc.terrainHeightView = chunk.heightMap->GetSrv()->BindlessView();
                        list.SetUnorderedAccessView(0, m_perBladeDataBuffer.get());
                        list.SetRootConstant<PerBladeComputeRootConstant>(1, m_pbcrc);
                        list.SetBindlessHeap(2);

                        list.GetList()->Dispatch(MAXBLADESPERCHUNK / 64, 1, 1);
                    }

                    list.EndRender();

                    m_perBladeDataBuffer->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    m_recomputeGrassBlades = false;
                }
            });
    }

    /// <summary>
    /// The reason I clear the instance count via a compute shader is because doing it via clear uav uint had specific
    /// requirments for how the heap should be created which required me to create another heap specifically for
    /// clearing. Compute made it straight forward and simple without cluttering the code.
    /// </summary>
    void IndirectGrass::AddClearCounterPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<ClearCounterData>();
        // Dependency with per blade grass computation pass
        auto* dependency = rg.GetPassData<ClearCounterData, PerBladePassData>();

        rg.AddPass<ClearCounterData>(
            "Clear counter pass", PassType::Compute, [&renderer, this](ClearCounterData& countData, CommandList& list) {
                auto context = engine.GetGfxContext();
                UINT frameIndex = context->GetBackBufferIndex();

                // Set all uav buffers to the correct state
                m_culledInstancesBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                m_instanceCountBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                m_drawCommandsBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeClearCounter.slang");

                std::vector<Uniform> uniforms;
                Uniform instanceCountBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(instanceCountBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Clear counter pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Clear counter pass");

                list.SetUnorderedAccessView(0, m_instanceCountBuffer[frameIndex].get());

                list.GetList()->Dispatch(1, 1, 1);
                list.EndRender();

                // TODO abstract resource barriers away
                CD3DX12_RESOURCE_BARRIER uavBarrier =
                    CD3DX12_RESOURCE_BARRIER::UAV(m_instanceCountBuffer[frameIndex]->GetBuffer());
                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }

    /// <summary>
    /// GPU frustum culling combined with indirect rendering.
    /// This pass also calculates which LOD's to use for the grass blades
    /// and writes the amount of instances to the instance count buffer
    /// which will be used in the AddIndirectDrawCommandsPass.
    /// </summary>
    void IndirectGrass::AddGrassCulling(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<GrassCullData>();

        // The dependency makes sure the clear counter pass is executed before executing this pass
        auto dependency = rg.GetPassData<GrassCullData, ClearCounterData>();

        rg.AddPass<GrassCullData>(
            "Grass culling pass", PassType::Compute, [&renderer, this](GrassCullData& cullingData, CommandList& list) {
                auto context = engine.GetGfxContext();

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeGrassCulling.slang");

                std::vector<Uniform> uniforms;

                // Per frame frustum data
                Uniform frustumData{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(frustumData);

                // Grass instance data
                Uniform grassInstanceData{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(grassInstanceData);

                // Culled read write data
                Uniform cullingDataBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(cullingDataBuffer);

                // Byte adress buffer for instance count
                Uniform instanceCounter{1, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(instanceCounter);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Grass culling pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("Grass culling pass");

                UINT frameIndex = context->GetBackBufferIndex();

                // Set frame data
                list.SetConstantBufferView(0, m_frustumBuffer[frameIndex].get());
                list.SetShaderResourceView(1, m_perBladeDataBuffer.get());
                list.SetUnorderedAccessView(2, m_culledInstancesBuffer[frameIndex].get());
                list.SetUnorderedAccessView(3, m_instanceCountBuffer[frameIndex].get());

                list.GetList()->Dispatch(((MAXGRASSBLADES + 63) / 64), 1, 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER barriers[1] = {};

                // Barrier for instance count buffer and culled instances
                barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barriers[0].UAV.pResource = m_instanceCountBuffer[frameIndex]->GetBuffer();

                list.GetList()->ResourceBarrier(1, barriers);

                m_culledInstancesBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                cullingData.CulledBuffer = m_culledInstancesBuffer[frameIndex];
            });
    }

    /// <summary>
    /// Creates N amount of draw commands depending on the amount of LOD's the grass blades.
    /// </summary>
    void IndirectGrass::AddIndirectDrawCommandsPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<IndirectCommandsData>();
        auto cullData = rg.GetPassData<IndirectCommandsData, GrassCullData>();

        rg.AddPass<IndirectCommandsData>(
            "Indirect command creation pass",
            PassType::Compute,
            [&renderer, this](const IndirectCommandsData& indirectCmds, CommandList& list) {
                auto context = engine.GetGfxContext();

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeIndirectCommands.slang");

                std::vector<Uniform> uniforms;
                Uniform instanceCountBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(instanceCountBuffer);

                Uniform drawCommandBuffer{1, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(drawCommandBuffer);

                auto& pipeline = renderer.GetOrCreatePipeline(
                    "Indirect command creation pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("Draw command creation pass");

                int frameIndex = context->GetBackBufferIndex();
                list.SetUnorderedAccessView(0, m_instanceCountBuffer[frameIndex].get());
                list.SetUnorderedAccessView(1, m_drawCommandsBuffer[frameIndex].get());

                list.GetList()->Dispatch(1, 1, 1);

                list.EndRender();

                m_instanceCountBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                m_drawCommandsBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            });
    }

    void IndirectGrass::AddRenderGrass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<RenderGrassData>();
        auto indirectPass = rg.GetPassData<RenderGrassData, IndirectCommandsData>();
        auto* pcgPass = rg.GetPassData<RenderGrassData, DrawTerrainPassData>();

        passData->albedoRoughnessTexture = pcgPass->albedoRoughnessTexture;
        passData->normalMetallicTexture = pcgPass->normalMetallicTexture;
        passData->emissiveTexture = pcgPass->emissiveTexture;
        passData->depthTexture = pcgPass->depthTexture;

        rg.AddPass<RenderGrassData>(
            "Grass render pass", PassType::Graphics, [&renderer, this](const RenderGrassData& grassData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/VertGrass.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/FragGrass.slang");
                settings.DepthStencilState.DepthEnable = true;

                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(InputElement("COORDS", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("SWAY", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3) + sizeof(float)));

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Albedo
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Normal
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Emissive

                settings.RasterizerState.CullMode = CullMode::None;

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(GrassRC)};
                uniforms.emplace_back(rootConstant);

                Uniform grassBladeData{0, 0, RootParams::RootResourceType::ShaderResourceView, 0, D3D12_SHADER_VISIBILITY_VERTEX};
                uniforms.emplace_back(grassBladeData);

                Uniform sceneData{1, 0, RootParams::RootResourceType::ConstantBufferView, 0};
                uniforms.emplace_back(sceneData);

                Uniform culledInstanceData{1, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(culledInstanceData);

                Uniform bindlessUni{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srvRange{};
                srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              2,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                bindlessUni.ranges.emplace_back(srvRange);

                uniforms.emplace_back(bindlessUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                staticSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                staticSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(staticSampler);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Grass render pass", PipelineStateType::Graphics, settings, uniforms);

                auto ecs = engine.GetECS();

                Camera* camera = GetActiveCamera();

                list.SetPipelineState(pipeline);
                list.BeginRender({grassData.albedoRoughnessTexture, grassData.normalMetallicTexture, grassData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 {grassData.depthTexture},
                                 DSClearOperation::Store,
                                 "Grass Pass");

                auto& gfxContext = engine.GetGfxContext();
                UINT frameIndex = gfxContext->GetBackBufferIndex();

                auto& transform = engine.GetECS()->GetComponent<Transform>(m_chunkEntity);
                if (camera)
                {
                    m_rc.matrix = camera->GetProjection() * camera->GetView() * transform.GetWorldMatrix();
                    m_rc.invMatrix = glm::transpose(glm::inverse(glm::mat3(transform.GetWorldMatrix())));
                }
                m_rc.bladeId = 0;

                bool first = false;
                for (auto [entity, chunk, transform] : ecs->GetRegistry().view<TerrainChunk, Transform>().each())
                {
                    if (!first)
                    {
                        m_rc.terrainView = chunk.heightMap->GetSrv()->BindlessView();
                        first = true;
                    }
                }

                list.SetRootConstant<GrassRC>(0, m_rc);
                list.SetShaderResourceView(1, m_perBladeDataBuffer.get());
                list.SetConstantBufferView(2, m_sceneData[gfxContext->GetBackBufferIndex()].get());

                if (m_culledInstancesBuffer[frameIndex]->GetBuffer())
                {
                    list.SetShaderResourceView(3, m_culledInstancesBuffer[frameIndex].get());
                }

                list.SetBindlessHeap(4);

                list.GetList()->IASetVertexBuffers(0, 1, &m_grassVertices->GetVBView()->View());
                list.GetList()->IASetIndexBuffer(&m_grassIndices->GetIBView()->View());

                // Indirect drawing to reduce cpu overhead and picking the correct LOD's. It executes a total of 3 draw
                // commands for all LOD's
                list.GetList()->ExecuteIndirect(m_commandSignature.Get(),
                                                m_lodAmount,
                                                m_drawCommandsBuffer[frameIndex]->GetBuffer(),
                                                0,
                                                m_instanceCountBuffer[frameIndex]->GetBuffer(),
                                                0);

                list.EndRender();
            });
    }

    void IndirectGrass::UpdateFrustumData(const int frameIndex)
    {
        auto ecs = engine.GetECS();
        auto& cameras = ecs->View<Camera>();

        FrustumBuffer FrustumData{};

        // Loop over all camera's TODO make the code run for each camera entity
        for (auto& cameraEntity : cameras)
        {
            if (ecs->HasComponent<Camera>(cameraEntity))
            {
                auto& cam = ecs->GetComponent<Camera>(cameraEntity);

                FrustumData.viewProj = cam.GetView() * cam.GetProjection();
                FrustumData.cameraPos = cam.GetPosition();
            }
        }

        // Extracting the frustum data from the view and proj
        FrustumData.frustumPlanes[0] = glm::row(FrustumData.viewProj, 3) + glm::row(FrustumData.viewProj, 0); // Left
        FrustumData.frustumPlanes[1] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 0); // Right
        FrustumData.frustumPlanes[2] = glm::row(FrustumData.viewProj, 3) + glm::row(FrustumData.viewProj, 1); // Bottom
        FrustumData.frustumPlanes[3] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 1); // Top
        FrustumData.frustumPlanes[4] = glm::row(FrustumData.viewProj, 2);                                     // Near
        FrustumData.frustumPlanes[5] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 2); // Far

        // Normalize frustum planes
        for (int i = 0; i < 6; i++)
        {
            FrustumData.frustumPlanes[i] = glm::normalize(FrustumData.frustumPlanes[i]);
        }

        // TODO make slider for LOD change in imgui
        FrustumData.lod0 = 15.0f;
        FrustumData.lod1 = 30.0f;
        FrustumData.lod2 = 50.0f;
        FrustumData.maxDistance = 70.0f;
        FrustumData.lodBlendRange = 5.0f;

        m_frustumBuffer[frameIndex]->Allocate(&FrustumData);
    }

    void IndirectGrass::CreateGrassMeshes()
    {
        std::vector<GrassVertex> grassVertices{};
        grassVertices.reserve(21);
        std::vector<uint32_t> grassIndices{};
        grassIndices.reserve(45);

        // Grass blade vertice data | position, 1D coordinates and sway
        /// First lod grass blade
        grassVertices.push_back({{-0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{-0.03f, 0.2f, 0.0f}, 0.2f, 0.2f});
        grassVertices.push_back({{0.03f, 0.2f, 0.0f}, 0.2f, 0.2f});
        grassVertices.push_back({{-0.03f, 0.4f, 0.0f}, 0.4f, 0.4f});
        grassVertices.push_back({{0.03f, 0.4f, 0.0f}, 0.4f, 0.4f});
        grassVertices.push_back({{-0.03f, 0.6f, 0.0f}, 0.6f, 0.6f});
        grassVertices.push_back({{0.03f, 0.6f, 0.0f}, 0.6f, 0.6f});
        grassVertices.push_back({{0.0f, 0.75f, 0.0f}, 1.0f, 1.0f});

        grassIndices.insert(grassIndices.end(), {0, 1, 2, 1, 3, 2}); // First quad
        grassIndices.insert(grassIndices.end(), {2, 3, 4, 3, 5, 4}); // Second quad
        grassIndices.insert(grassIndices.end(), {4, 5, 6, 5, 7, 6}); // Third quad
        grassIndices.insert(grassIndices.end(), {6, 7, 8});          // Top triangle

        /// Second lod grass blade
        grassVertices.push_back({{-0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{-0.03f, 0.3f, 0.0f}, 0.3f, 0.3f});
        grassVertices.push_back({{0.03f, 0.3f, 0.0f}, 0.3f, 0.3f});
        grassVertices.push_back({{-0.03f, 0.6f, 0.0f}, 0.6f, 0.6f});
        grassVertices.push_back({{0.03f, 0.6f, 0.0f}, 0.6f, 0.6f});
        grassVertices.push_back({{0.0f, 0.75f, 0.0f}, 1.0f, 1.0f});

        grassIndices.insert(grassIndices.end(), {0, 1, 2, 1, 3, 2}); // First quad
        grassIndices.insert(grassIndices.end(), {2, 3, 4, 3, 5, 4}); // Second quad
        grassIndices.insert(grassIndices.end(), {4, 5, 6});          // Top triangle

        /// Third lod grass blade
        grassVertices.push_back({{-0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{0.03f, 0.0f, 0.0f}, 0.0f, 0.0f});
        grassVertices.push_back({{-0.03f, 0.5f, 0.0f}, 0.5f, 0.5f});
        grassVertices.push_back({{0.03f, 0.5f, 0.0f}, 0.5f, 0.5f});
        grassVertices.push_back({{0.0f, 0.75f, 0.0f}, 1.0f, 1.0f});

        grassIndices.insert(grassIndices.end(), {0, 1, 2, 1, 3, 2}); // Quad
        grassIndices.insert(grassIndices.end(), {2, 3, 4});          // Top triangle

        BufferDesc desc{};
        m_grassVertices = std::make_unique<Buffer>(desc);
        m_grassVertices->CreateVertexBuffer<GrassVertex>(grassVertices);

        m_grassIndices = std::make_unique<Buffer>(desc);
        m_grassIndices->CreateIndexBuffer(grassIndices);
    }
} // namespace Wild
