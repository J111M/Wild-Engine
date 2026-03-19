#include "Renderer/Passes/DeferredPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Model.hpp"

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

        engine.GetSceneManager()->AddScene("Sponza", []() {
            auto ecs = engine.GetECS();
            auto entity = ecs->CreateEntity();
            ecs->AddComponent<SceneObject>(entity);
            auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
            ecs->AddComponent<Model>(entity, "Assets/Models/bistro/bistro/bistro.gltf", entity);
            transform.SetScale(glm::vec3(1, 1, 1));
            transform.SetPosition(glm::vec3(70, 1, 70));
            transform.Name = "Sponza";
        });

        engine.GetSceneManager()->LoadScene("Main Scene");
    }

    void DeferredPass::Update(const float dt) {}

    void DeferredPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DeferredPassData>();
        auto* grassData = rg.GetPassData<DeferredPassData, RenderGrassData>();

        passData->albedoRoughnessTexture = grassData->albedoRoughnessTexture;
        passData->normalMetallicTexture = grassData->normalMetallicTexture;
        passData->emissiveTexture = grassData->emissiveTexture;
        passData->depthTexture = grassData->depthTexture;

        rg.AddPass<DeferredPassData>(
            "Deferred pass", PassType::Graphics, [&renderer, this](const DeferredPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DeferredVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DeferredFrag.slang");
                settings.DepthStencilState.DepthEnable = true;

                // Setting up the input layout
                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TANGENT", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(glm::vec3) * 3 + sizeof(glm::vec2)));

                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Albedo
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Normal
                settings.renderTargetsFormat.push_back(DXGI_FORMAT_R8G8B8A8_UNORM); // Emissive

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(RootConstant)};
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

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 {passData.depthTexture},
                                 DSClearOperation::Store,
                                 "Deferred pass");

                auto meshes = ecs->GetRegistry().view<Transform, MeshComponent>();
                for (auto&& [entity, trans, meshComponent] : meshes.each())
                {
                    m_rc = RootConstant{};

                    if (camera)
                    {
                        m_rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
                        m_rc.invMatrix = glm::transpose(glm::inverse(glm::mat3(trans.GetWorldMatrix())));
                    }

                    auto& mesh = meshComponent.mesh;
                    if (!mesh) continue;

                    auto material = mesh->GetMaterial();
                    if (material.m_albedo) m_rc.albedoView = material.m_albedo->GetSrv()->BindlessView();

                    if (material.m_normal) m_rc.normalView = material.m_normal->GetSrv()->BindlessView();

                    if (material.m_roughnessMetallic)
                        m_rc.roughnessMetallicView = material.m_roughnessMetallic->GetSrv()->BindlessView();

                    if (material.m_emissive) m_rc.emissiveView = material.m_emissive->GetSrv()->BindlessView();

                    list.SetRootConstant<RootConstant>(0, m_rc);

                    list.SetBindlessHeap(1);

                    list.GetList()->IASetVertexBuffers(0, 1, &mesh->GetVertexBuffer()->GetVBView()->View());

                    if (mesh->HasIndexBuffer())
                    {
                        list.GetList()->IASetIndexBuffer(&mesh->GetIndexBuffer()->GetIBView()->View());
                        list.GetList()->DrawIndexedInstanced(mesh->GetDrawCount(), 1, 0, 0, 0);
                    }
                    else
                    {
                        list.GetList()->DrawInstanced(mesh->GetDrawCount(), 1, 0, 0);
                    }
                }

                list.EndRender();
            });
    }
} // namespace Wild
