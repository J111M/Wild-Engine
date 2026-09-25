#include "Renderer/Passes/DeferredPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Model.hpp"

#include <cstddef>

namespace Wild
{
    DeferredPass::DeferredPass()
    {
        // TODO make scene storing via json parsing
        engine.GetSceneManager()->AddScene("Main Scene", []() {
            auto ecs = engine.GetECS();

            glm::vec3 palmPositions[5] = {glm::vec3(16.251, -5.655, 20),
                                          glm::vec3(-27.567, -6.976, -5.361),
                                          glm::vec3(-25.417, -6.732, -24.320),
                                          glm::vec3(4.529, -1.24, -7.303),
                                          glm::vec3(-5.261, -3.097, 14.194)};

            for (size_t i = 0; i < 5; i++)
            {
                auto entity = ecs->CreateEntity();
                ecs->AddComponent<SceneObject>(entity);
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                ecs->AddComponent<Model>(entity, "Assets/Models/palmtree/quiver_tree_02_2k.gltf", entity);
                transform.SetScale(glm::vec3(10, 10, 10));
                transform.SetPosition(palmPositions[i]);
                transform.Name = "Palm tree";
            }

            {
                auto entity = ecs->CreateEntity();
                ecs->AddComponent<SceneObject>(entity);
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                ecs->AddComponent<Model>(entity, "Assets/Models/ship/ship_pinnace_2k.gltf", entity);
                transform.SetScale(glm::vec3(1.5, 1.5, 1.5));
                transform.SetPosition(glm::vec3(70, -9, 40));
                transform.Name = "Ship";
            }

            {
                auto entity = ecs->CreateEntity();
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                auto& light = ecs->AddComponent<PointLight>(entity);

                transform.Name = std::string("Point light");

                transform.SetPosition(glm::vec3(72.157, 5.284, 13.881));
                light.position = transform.GetPosition();

                light.colorIntensity = glm::vec4(glm::vec3(0.0, 0.6, 0.4), 1200.0f);
            }

            {
                auto entity = ecs->CreateEntity();
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                auto& light = ecs->AddComponent<PointLight>(entity);

                transform.Name = std::string("Point light");

                transform.SetPosition(glm::vec3(64.445, 5.685, 13.886));
                light.position = transform.GetPosition();

                light.colorIntensity = glm::vec4(glm::vec3(0.0, 0.8, 0.5), 1800.0f);
            }

            {
                auto entity = ecs->CreateEntity();
                auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                auto& light = ecs->AddComponent<PointLight>(entity);

                transform.Name = std::string("Point light");

                transform.SetPosition(glm::vec3(69.377, -5.387, 44.431));
                light.position = transform.GetPosition();

                light.colorIntensity = glm::vec4(glm::vec3(0.0, 0.8, 0.2), 5971.0f);
            }
        });

        engine.GetSceneManager()->AddScene("Bistro", []() {
            auto ecs = engine.GetECS();
            auto entity = ecs->CreateEntity();
            ecs->AddComponent<SceneObject>(entity);
            auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
            ecs->AddComponent<Model>(entity, "Assets/Models/bistro/bistro/bistro.gltf", entity);
            transform.SetScale(glm::vec3(1, 1, 1));
            transform.SetPosition(glm::vec3(70, 1, 70));
            transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
            transform.Name = "Bistro";
        });

        engine.GetSceneManager()->AddScene("Sponza", []() {
            auto ecs = engine.GetECS();
            auto entity = ecs->CreateEntity();
            ecs->AddComponent<SceneObject>(entity);
            auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
            ecs->AddComponent<Model>(entity, "Assets/Models/Sponza/glTF/Sponza.gltf", entity);
            transform.SetScale(glm::vec3(1, 1, 1));
            transform.SetPosition(glm::vec3(70, 1, 70));
            // transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
            transform.Name = "Sponza";
        });

        engine.GetSceneManager()->LoadScene("Sponza");
    }

    void DeferredPass::Update(const float dt)
    {
        const bool meshShaderPathAvailable = SupportsMeshShaderPath();
        const bool clusterDebugAvailable = SupportsClusterDebugView();

        engine.GetImGui()->AddPanel("Geometry settings", [this, meshShaderPathAvailable, clusterDebugAvailable]() {
            ImGui::BeginDisabled(!clusterDebugAvailable);
            ImGui::Checkbox("Mesh cluster debug view", &m_meshletDebugView);
            ImGui::EndDisabled();

            if (!clusterDebugAvailable)
            {
                ImGui::TextUnformatted("This device builds no meshlets, there are no clusters to colour");
                return;
            }

            if (!meshShaderPathAvailable)
            {
                ImGui::TextUnformatted("Mesh shaders unsupported, running the vertex path");
                return;
            }

            ImGui::Checkbox("Force vertex path", &m_forceVertexPath);

            ImGui::BeginDisabled(m_forceVertexPath);
            ImGui::Checkbox("Use amplification shader", &m_useAmplificationShader);
            ImGui::EndDisabled();

            ImGui::BeginDisabled(m_forceVertexPath || !m_useAmplificationShader);
            ImGui::Checkbox("Freeze frustum culling", &m_freezeCulling);
            ImGui::EndDisabled();
        });
    }

    bool DeferredPass::SupportsMeshShaderPath() const
    {
        const auto& capabilities = engine.GetGfxContext()->GetCapabilities();
        return capabilities.CheckMeshShaderSupport(MeshShaderSupport::Tier1) &&
               capabilities.CheckResourceBindingSupport(ResourceBindingSupport::Tier3);
    }

    bool DeferredPass::SupportsClusterDebugView() const
    {
        const auto& capabilities = engine.GetGfxContext()->GetCapabilities();
        return capabilities.SupportsMeshShaders() &&
               capabilities.CheckResourceBindingSupport(ResourceBindingSupport::Tier3);
    }

    void DeferredPass::IndirectPreparePass(Renderer& renderer, RenderGraph& rg) {}

    void DeferredPass::DeferredMeshShaderPass(Renderer& renderer, RenderGraph& rg, const bool useAmplification)
    {
        const char* passName = useAmplification ? "Deferred amplification pass" : "Deferred mesh shader pass";

        rg.AddPass<DeferredPassData>(
            passName, PassType::MeshShader,
            [&renderer, this, useAmplification, passName](const DeferredPassData& passData, CommandList& list) {
                // Meshlets handled by one group of the first stage, the task shader culls 64 meshlets per group
                const uint32_t meshletsPerGroup = useAmplification ? 64u : 1u;

                PipelineStateSettings settings{};
                if (useAmplification)
                {
                    settings.shaderState.amplificationShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Geometry/DeferredAmplificationShader.slang");
                    settings.shaderState.meshShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Geometry/DeferredMeshShader.slang");
                }
                else
                {
                    settings.shaderState.meshShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Geometry/DeferredMeshShader.slang");
                }
                settings.shaderState.fragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DeferredFrag.slang");
                settings.depthStencilState.depthEnable = true;

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);     // Albedo
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R16G16B16A16_UNORM); // Normal
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);     // Emissive

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DeferredMeshShaderRootConstants)};
                uniforms.emplace_back(rootConstant);

                Uniform bindlessUni{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srvRange{};
                srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 0, 1);
                bindlessUni.ranges.emplace_back(srvRange);
                bindlessUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                uniforms.emplace_back(bindlessUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                uniforms.emplace_back(staticSampler);

                auto pipeline = renderer.GetOrCreatePipeline(passName, PipelineStateType::MeshPipeline, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store,
                                 passName);
                list.SetBindlessHeap(1);

                Camera* camera = GetActiveCamera();

                if (camera && !m_freezeCulling) m_cullViewProjection = camera->GetProjection() * camera->GetView();

                const bool meshletDebug = m_meshletDebugView && SupportsClusterDebugView();

                auto meshes = engine.GetECS()->GetRegistry().view<Transform, MeshComponent>();
                for (auto&& [entity, trans, meshComponent] : meshes.each())
                {
                    if (!meshComponent.mesh) continue;
                    auto& mesh = *meshComponent.mesh;
                    if (!mesh.GetMeshlets()) continue;
                    const auto& meshlets = mesh.GetMeshlets()->GetMeshletData();
                    if (meshlets.meshletCount == 0) continue;

                    DeferredMeshShaderRootConstants rc{};
                    if (camera)
                    {
                        rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
                        rc.invMatrix = glm::transpose(glm::inverse(glm::mat3(trans.GetWorldMatrix())));
                    }
                    rc.cullMatrix = m_cullViewProjection * trans.GetWorldMatrix();

                    const auto& material = mesh.GetMaterial();
                    if (material.m_albedo) rc.albedoView = material.m_albedo->GetSrv()->BindlessView();
                    if (material.m_normal) rc.normalView = material.m_normal->GetSrv()->BindlessView();
                    if (material.m_roughnessMetallic)
                        rc.roughnessMetallicView = material.m_roughnessMetallic->GetSrv()->BindlessView();
                    if (material.m_emissive) rc.emissiveView = material.m_emissive->GetSrv()->BindlessView();

                    rc.meshletDebugView = meshletDebug ? 1u : 0u;

                    auto vertexBuffer = mesh.GetVertexBuffer();
                    vertexBuffer->Transition(list, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
                                                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    meshlets.meshletBuffer->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    meshlets.uniqueVertexIndexBuffer->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    meshlets.primitiveIndexBuffer->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    rc.vertexDataBuffer = vertexBuffer->GetSRView()->View();
                    rc.meshletBufferView = meshlets.meshletBuffer->GetSRView()->View();
                    rc.meshletVertexBufferView = meshlets.uniqueVertexIndexBuffer->GetSRView()->View();
                    rc.primIndexBufferView = meshlets.primitiveIndexBuffer->GetSRView()->View();

                    // A dispatch is capped at 65535 groups, so large meshes are split into several slices
                    const uint32_t maxMeshletsPerDispatch = 65535u * meshletsPerGroup;
                    for (uint32_t offset = 0; offset < meshlets.meshletCount;)
                    {
                        uint32_t count = std::min(meshlets.meshletCount - offset, maxMeshletsPerDispatch);
                        rc.meshletOffset = offset;
                        rc.meshletCount = count;
                        list.SetRootConstant(0, rc);
                        list.Dispatch((count + meshletsPerGroup - 1) / meshletsPerGroup);
                        offset += count;
                    }
                }

                list.EndRender();
            });
    }

    void DeferredPass::DeferredVertexPass(Renderer& renderer, RenderGraph& rg)
    {
        rg.AddPass<DeferredPassData>(
            "Deferred pass", PassType::Graphics, [&renderer, this](const DeferredPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.shaderState.vertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DeferredVert.slang");
                settings.shaderState.fragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DeferredFrag.slang");
                settings.depthStencilState.depthEnable = true;

                // Setting up the input layout
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, offsetof(Vertex, position)));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, offsetof(Vertex, color)));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, offsetof(Vertex, normal)));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, offsetof(Vertex, uv)));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("TANGENT", DXGI_FORMAT_R32G32B32A32_FLOAT, offsetof(Vertex, tangent)));

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);     // Albedo
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R16G16B16A16_UNORM); // Normal
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);     // Emissive

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DeferredRootConstants)};
                uniforms.emplace_back(rootConstant);

                Uniform bindlessUni{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srvRange{};
                srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                              UINT_MAX,
                              0,
                              0,
                              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                bindlessUni.ranges.emplace_back(srvRange);
                bindlessUni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;

                uniforms.emplace_back(bindlessUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                uniforms.emplace_back(staticSampler);

                auto& pipeline = renderer.GetOrCreatePipeline("Deferred pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                auto ecs = engine.GetECS();

                Camera* camera = GetActiveCamera();

                const bool clusterDebug = m_meshletDebugView && SupportsClusterDebugView();

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 {passData.depthTexture},
                                 DSClearOperation::Store,
                                 "Deferred pass");

                auto meshes = ecs->GetRegistry().view<Transform, MeshComponent>();
                for (auto&& [entity, trans, meshComponent] : meshes.each())
                {
                    if (!meshComponent.mesh) continue;
                    auto& mesh = *meshComponent.mesh;

                    m_rc = DeferredRootConstants{};

                    if (camera)
                    {
                        m_rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
                        m_rc.invMatrix = glm::transpose(glm::inverse(glm::mat3(trans.GetWorldMatrix())));
                    }

                    auto material = mesh.GetMaterial();
                    if (material.m_albedo) m_rc.albedoView = material.m_albedo->GetSrv()->BindlessView();

                    if (material.m_normal) m_rc.normalView = material.m_normal->GetSrv()->BindlessView();

                    if (material.m_roughnessMetallic)
                        m_rc.roughnessMetallicView = material.m_roughnessMetallic->GetSrv()->BindlessView();

                    if (material.m_emissive) m_rc.emissiveView = material.m_emissive->GetSrv()->BindlessView();

                    if (clusterDebug && mesh.GetMeshlets())
                    {
                        const auto& meshlets = mesh.GetMeshlets()->GetMeshletData();
                        if (meshlets.vertexMeshletIdBuffer)
                        {
                            meshlets.vertexMeshletIdBuffer->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                            m_rc.vertexMeshletIdBufferView = meshlets.vertexMeshletIdBuffer->GetSRView()->View();
                            m_rc.meshletDebugView = 1u;
                        }
                    }

                    list.SetRootConstant<DeferredRootConstants>(0, m_rc);

                    list.SetBindlessHeap(1);

                    list.GetList()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

                    if (mesh.HasIndexBuffer())
                    {
                        list.GetList()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
                        list.GetList()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
                    }
                    else
                    {
                        list.GetList()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
                    }
                }

                list.EndRender();
            });
    }

    void DeferredPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DeferredPassData>();
        auto* grassData = rg.GetPassData<DeferredPassData, RenderGrassData>();

        passData->albedoRoughnessTexture = grassData->albedoRoughnessTexture;
        passData->normalMetallicTexture = grassData->normalMetallicTexture;
        passData->emissiveTexture = grassData->emissiveTexture;
        passData->depthTexture = grassData->depthTexture;

        if (SupportsMeshShaderPath() && !m_forceVertexPath)
            DeferredMeshShaderPass(renderer, rg, m_useAmplificationShader);
        else
            DeferredVertexPass(renderer, rg);
    }
} // namespace Wild
