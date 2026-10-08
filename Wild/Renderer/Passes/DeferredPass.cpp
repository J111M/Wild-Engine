#include "Renderer/Passes/DeferredPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Model.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Wild
{
    DeferredPass::DeferredPass()
    {
        engine.GetSceneManager()->AddScene("Bistro", []() {
            auto ecs = engine.GetECS();
            auto entity = ecs->CreateEntity();
            ecs->AddComponent<SceneObject>(entity);
            auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
            ecs->AddComponent<Model>(entity, "Assets/Models/bistro/bistro/bistro.gltf", entity);
            transform.SetScale(glm::vec3(1, 1, 1));
            transform.SetPosition(glm::vec3(0, 1, 0));
            transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
            transform.Name = "Bistro";
        });

        engine.GetSceneManager()->AddScene("Culling test", []() {
            auto ecs = engine.GetECS();
            for (size_t x = 0; x < 25; x++)
            {
                for (size_t y = 0; y < 25; y++)
                {
                    for (size_t z = 0; z < 25; z++)
                    {
                        auto entity = ecs->CreateEntity();
                        ecs->AddComponent<SceneObject>(entity);
                        auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
                        ecs->AddComponent<Model>(entity, "Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", entity);
                        transform.SetScale(glm::vec3(1, 1, 1));
                        transform.SetPosition(glm::vec3(x * 2, y * 2, z * 2));
                        // transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
                        transform.Name = "Damaged helmet" + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z);
                    }
                }
            }
        });

        engine.GetSceneManager()->AddScene("Sponza", []() {
            auto ecs = engine.GetECS();
            auto entity = ecs->CreateEntity();
            ecs->AddComponent<SceneObject>(entity);
            auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
            ecs->AddComponent<Model>(entity, "Assets/Models/Sponza/glTF/Sponza.gltf", entity);
            transform.SetScale(glm::vec3(1, 1, 1));
            transform.SetPosition(glm::vec3(0, 1, 0));
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
        return capabilities.SupportsMeshShaders() && capabilities.CheckResourceBindingSupport(ResourceBindingSupport::Tier3);
    }

    void DeferredPass::OcclusionPrepass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<OcclusionPrepassData>();

        // Persists between frames through the transient cache, filled by the previous depth copy pass at the end of the frame
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            // Full chain
            desc.mips = static_cast<uint32_t>(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
            desc.format = DXGI_FORMAT_R32_FLOAT;
            desc.name = "Previous frame depth";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
            passData->previousDepthTexture = rg.CreateTransientTexture("PreviousFrameDepth", desc);
            passData->hzbMipCount = desc.mips;
        }

        // Render large objects to the depth texture since previous frame data might not always be the same
         //rg.AddPass<OcclusionPrepassData>(
         //   "Occlusion depth render prepass", PassType::Graphics, [&renderer, this](const OcclusionPrepassData& passData, CommandList& list) {
         //       //passData.previousDepthTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

         //   });

        // Build the hierarchical z buffer from the previous frames depth, used to test against occluded objects
        rg.AddPass<OcclusionPrepassData>(
            "Create mip chain pass", PassType::Compute, [&renderer, this](const OcclusionPrepassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.shaderState.computeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Geometry/OcclusionCulling/DepthMipChain.slang");

                std::vector<Uniform> uniforms;
                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DepthMipChainRootConstant)};
                uniforms.emplace_back(rootConstant);

                // Parent mip that is read from
                Uniform srcMipUav{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE srcRange{};
                srcRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                srcMipUav.ranges.emplace_back(srcRange);
                uniforms.emplace_back(srcMipUav);

                // Mip that is written to
                Uniform dstMipUav{0, 0, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE dstRange{};
                dstRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                dstMipUav.ranges.emplace_back(dstRange);
                uniforms.emplace_back(dstMipUav);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Create mip chain pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender();

                Texture* hzb = passData.previousDepthTexture;
                hzb->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                glm::uvec2 srcSize{hzb->Width(), hzb->Height()};
                for (uint32_t mip = 1; mip < passData.hzbMipCount; mip++)
                {
                    const glm::uvec2 dstSize = glm::max(srcSize / 2u, glm::uvec2(1u));

                    m_depthMipRc.srcSize = srcSize;
                    m_depthMipRc.dstSize = dstSize;
                    list.SetRootConstant<DepthMipChainRootConstant>(0, m_depthMipRc);
                    list.SetUnorderedAccessView(1, hzb, mip - 1);
                    list.SetUnorderedAccessView(2, hzb, mip);

                    list.Dispatch((dstSize.x + 7) / 8, (dstSize.y + 7) / 8, 1);

                    // The next mip reads what this dispatch wrote
                    D3D12_RESOURCE_BARRIER uavBarrier = {};
                    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    uavBarrier.UAV.pResource = hzb->GetResource();
                    list.GetList()->ResourceBarrier(1, &uavBarrier);

                    srcSize = dstSize;
                }

                list.EndRender();
            });
    }

    void DeferredPass::PreviousDepthCopyPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<PreviousDepthCopyPassData>();
        auto* deferredData = rg.GetPassData<PreviousDepthCopyPassData, DeferredPassData>();
        auto* occlusionData = rg.GetPassData<PreviousDepthCopyPassData, OcclusionPrepassData>();

        passData->depthTexture = deferredData->depthTexture;
        passData->previousDepthTexture = occlusionData->previousDepthTexture;

        rg.AddPass<PreviousDepthCopyPassData>(
            "Previous depth copy pass", PassType::Default, [](const PreviousDepthCopyPassData& passData, CommandList& list) {
                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_COPY_SOURCE);
                passData.previousDepthTexture->Transition(list, D3D12_RESOURCE_STATE_COPY_DEST);

                // Copy into mip 0 only, so the copy keeps working once the target has a mip chain
                D3D12_TEXTURE_COPY_LOCATION src{};
                src.pResource = passData.depthTexture->GetResource();
                src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                src.SubresourceIndex = 0;

                D3D12_TEXTURE_COPY_LOCATION dst{};
                dst.pResource = passData.previousDepthTexture->GetResource();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = 0;

                list.GetList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            });
    }

    void DeferredPass::DeferredMeshShaderPass(Renderer& renderer, RenderGraph& rg, const bool useAmplification)
    {
        const char* passName = useAmplification ? "Deferred amplification pass" : "Deferred mesh shader pass";

        rg.AddPass<DeferredPassData>(
            passName,
            PassType::MeshShader,
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

                // Read by the amplification stage for culling and by the mesh stage for the view projection
                Uniform sceneCameraUni{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(sceneCameraUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                uniforms.emplace_back(staticSampler);

                auto pipeline = renderer.GetOrCreatePipeline(passName, PipelineStateType::MeshPipeline, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store);
                list.SetBindlessHeap(1);
                list.SetConstantBufferView(2, GetSceneDataAddress());

                const glm::vec4 cameraPosition = GetSceneData().position;

                const bool meshletDebug = m_meshletDebugView && SupportsClusterDebugView();

                auto meshes = engine.GetECS()->GetRegistry().view<Transform, MeshComponent>();
                for (auto&& [entity, trans, meshComponent] : meshes.each())
                {
                    if (!meshComponent.mesh) continue;
                    auto& mesh = *meshComponent.mesh;
                    if (!mesh.GetMeshlets()) continue;
                    const auto& meshlets = mesh.GetMeshlets()->GetMeshletData();
                    if (meshlets.meshletCount == 0) continue;

                    const glm::mat4& world = trans.GetWorldMatrix();

                    DeferredMeshShaderRootConstants rc{};
                    rc.model = world;
                    rc.invTransposeModel = glm::mat4(glm::transpose(glm::inverse(glm::mat3(world))));
                    rc.cullCameraPosition = glm::vec3(glm::inverse(world) * cameraPosition);
                    const auto& material = mesh.GetMaterial();
                    if (material.m_albedo) rc.albedoView = material.m_albedo->GetSrv()->BindlessView();
                    if (material.m_normal) rc.normalView = material.m_normal->GetSrv()->BindlessView();
                    if (material.m_roughnessMetallic)
                        rc.roughnessMetallicView = material.m_roughnessMetallic->GetSrv()->BindlessView();
                    if (material.m_emissive) rc.emissiveView = material.m_emissive->GetSrv()->BindlessView();

                    rc.meshletDebugView = meshletDebug ? 1u : 0u;

                    auto vertexBuffer = mesh.GetVertexBuffer();
                    vertexBuffer->Transition(
                        list, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

                Uniform sceneCameraUni{1, 0, RootParams::RootResourceType::ConstantBufferView};
                sceneCameraUni.visibility = D3D12_SHADER_VISIBILITY_VERTEX;
                uniforms.emplace_back(sceneCameraUni);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                uniforms.emplace_back(staticSampler);

                auto& pipeline = renderer.GetOrCreatePipeline("Deferred pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                auto ecs = engine.GetECS();

                const bool clusterDebug = m_meshletDebugView && SupportsClusterDebugView();

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 {passData.depthTexture},
                                 DSClearOperation::Store);

                list.SetConstantBufferView(2, GetSceneDataAddress());

                auto meshes = ecs->GetRegistry().view<Transform, MeshComponent>();
                for (auto&& [entity, trans, meshComponent] : meshes.each())
                {
                    if (!meshComponent.mesh) continue;
                    auto& mesh = *meshComponent.mesh;

                    m_rc = DeferredRootConstants{};

                    m_rc.model = trans.GetWorldMatrix();
                    m_rc.invTransposeModel = glm::transpose(glm::inverse(glm::mat3(trans.GetWorldMatrix())));

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

        // Passes execute in the order they are added
        OcclusionPrepass(renderer, rg);
        rg.GetPassData<DeferredPassData, OcclusionPrepassData>();

        if (SupportsMeshShaderPath() && !m_forceVertexPath)
            DeferredMeshShaderPass(renderer, rg, m_useAmplificationShader);
        else
            DeferredVertexPass(renderer, rg);

       // PreviousDepthCopyPass(renderer, rg);
    }
} // namespace Wild
