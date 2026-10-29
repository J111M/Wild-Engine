#include "Renderer/Passes/DeferredPass.hpp"
#include "Renderer/Passes/GrassPass.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Systems/FrustumCullingSystem.hpp"

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

    uint32_t DeferredPass::CountSceneMeshlets() const
    {
        // Has to skip exactly the same entities as the mesh shader pass, otherwise the offsets drift apart
        uint32_t total = 0;
        auto meshes = engine.GetECS()->GetRegistry().view<Transform, MeshComponent>();
        for (auto&& [entity, trans, meshComponent] : meshes.each())
        {
            if (!meshComponent.mesh) continue;
            if (!meshComponent.mesh->GetMeshlets()) continue;
            total += meshComponent.mesh->GetMeshlets()->GetMeshletData().meshletCount;
        }
        return total;
    }

    void DeferredPass::EnsureMeshletVisibilityBuffer(uint32_t meshletCount)
    {
        if (meshletCount == 0 || meshletCount <= m_meshletVisibilityCapacity) return;

        // Grow with some headroom so adding a few entities does not stall the GPU every time
        const uint32_t capacity = std::max(meshletCount, m_meshletVisibilityCapacity + m_meshletVisibilityCapacity / 2);

        // The old buffer can still be in use by frames in flight
        if (m_meshletVisibilityBuffer) engine.GetGfxContext()->Flush();

        BufferDesc desc{};
        desc.size = static_cast<uint64_t>(capacity) * sizeof(uint32_t);
        desc.stride = sizeof(uint32_t);
        desc.usage = BufferUsage::ShaderRead | BufferUsage::ShaderWrite | BufferUsage::CopyDst;
        desc.access = MemoryAccess::GpuOnly;
        desc.name = "Meshlet visibility buffer";
        m_meshletVisibilityBuffer = std::make_unique<GPUBuffer>(desc);

        // Everything counts as visible on the first frame, so phase 1 draws it and phase 2 corrects the flags
        std::vector<uint32_t> visible(capacity, 1u);
        m_meshletVisibilityBuffer->UploadToGPU(visible.data());

        m_meshletVisibilityCapacity = capacity;
    }

    /// <summary>
    /// Draws meshlets that were visible previous frame
    /// </summary>
    void DeferredPass::DeferredEarlyPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DeferredEarlyPassData>();
        auto* grassData = rg.GetPassData<DeferredEarlyPassData, RenderGrassData>();

        passData->albedoRoughnessTexture = grassData->albedoRoughnessTexture;
        passData->normalMetallicTexture = grassData->normalMetallicTexture;
        passData->emissiveTexture = grassData->emissiveTexture;
        passData->depthTexture = grassData->depthTexture;

        EnsureMeshletVisibilityBuffer(CountSceneMeshlets());

        rg.AddPass<DeferredEarlyPassData>(
            "Deferred early pass",
            PassType::MeshShader,
            [&renderer, this](const DeferredEarlyPassData& passData, CommandList& list) {
                RecordMeshShaderDraws(renderer, passData, list, true, false, nullptr);
            });
    }

    /// <summary>
    /// Builds a mip chain of the depth rendered by the early pass
    /// </summary>
    void DeferredPass::BuildHZBPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<HZBPassData>();
        auto* copyData = rg.GetPassData<HZBPassData, HZBDepthCopyPassData>();

        passData->hzbTexture = copyData->hzbTexture;
        passData->hzbMipCount = copyData->hzbMipCount;

        // Render large objects to the depth texture since previous frame data might not always be the same
        // rg.AddPass<HZBPassData>(
        //   "Occlusion depth render prepass", PassType::Graphics, [&renderer, this](const HZBPassData& passData,
        //   CommandList& list) {
        //       //passData.hzbTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        //   });

        // Build the hierarchical z buffer from the early pass depth, used to test against occluded objects
        rg.AddPass<HZBPassData>(
            "HZB build pass", PassType::Compute, [&renderer, this](const HZBPassData& passData, CommandList& list) {
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

                auto& pipeline = renderer.GetOrCreatePipeline("HZB build pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender();

                Texture* hzb = passData.hzbTexture;
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

    /// <summary>
    /// Draws meshlets that became visible this frame and updates the visibility for the next frame
    /// </summary>
    void DeferredPass::DeferredLatePass(Renderer& renderer, RenderGraph& rg)
    {
        const HZBPassData* hzbData = rg.GetPassData<DeferredPassData, HZBPassData>();

        rg.AddPass<DeferredPassData>(
            "Deferred late pass",
            PassType::MeshShader,
            [&renderer, this, hzbData](const DeferredPassData& passData, CommandList& list) {
                hzbData->hzbTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                // The early pass read the visibility flags that this pass overwrites
                if (m_meshletVisibilityBuffer)
                {
                    D3D12_RESOURCE_BARRIER uavBarrier = {};
                    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    uavBarrier.UAV.pResource = m_meshletVisibilityBuffer->GetBuffer();
                    list.GetList()->ResourceBarrier(1, &uavBarrier);
                }

                RecordMeshShaderDraws(renderer, passData, list, true, true, hzbData);
            });
    }

    void DeferredPass::HZBDepthCopyPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<HZBDepthCopyPassData>();
        auto* deferredData = rg.GetPassData<HZBDepthCopyPassData, DeferredEarlyPassData>();

        passData->depthTexture = deferredData->depthTexture;

        // Mip 0 is filled by this pass, the rest of the chain is built from it by the HZB build pass
        {
            TextureDesc desc;
            desc.width = engine.GetGfxContext()->GetWidth();
            desc.height = engine.GetGfxContext()->GetHeight();
            // Full chain
            desc.mips = static_cast<uint32_t>(std::floor(std::log2(std::max(desc.width, desc.height)))) + 1;
            // Use R32 becuase this works better with UAV
            desc.format = DXGI_FORMAT_R32_FLOAT;
            desc.name = "HZB texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::shaderResource | TextureDesc::readWrite);
            passData->hzbTexture = rg.CreateTransientTexture("HZBTexture", desc);
            passData->hzbMipCount = desc.mips;
        }

        rg.AddPass<HZBDepthCopyPassData>(
            "HZB depth copy pass", PassType::Default, [](const HZBDepthCopyPassData& passData, CommandList& list) {
                passData.depthTexture->Transition(list, D3D12_RESOURCE_STATE_COPY_SOURCE);
                passData.hzbTexture->Transition(list, D3D12_RESOURCE_STATE_COPY_DEST);

                // Copy into mip 0 only, so the copy keeps working once the target has a mip chain
                D3D12_TEXTURE_COPY_LOCATION src{};
                src.pResource = passData.depthTexture->GetResource();
                src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                src.SubresourceIndex = 0;

                D3D12_TEXTURE_COPY_LOCATION dst{};
                dst.pResource = passData.hzbTexture->GetResource();
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
            [&renderer, this, useAmplification](const DeferredPassData& passData, CommandList& list) {
                RecordMeshShaderDraws(renderer, passData, list, useAmplification, false, nullptr);
            });
    }

    void DeferredPass::RecordMeshShaderDraws(Renderer& renderer, const DeferredPassData& passData, CommandList& list,
                                             const bool useAmplification, const bool latePass, const HZBPassData* hzbData)
    {
        // Each phase compiles its own amplification shader variant, so each needs its own pipeline
        const char* passName = !useAmplification ? "Deferred mesh shader pass"
            : latePass                           ? "Deferred late amplification pass"
                                                 : "Deferred early amplification pass";

        // Meshlets handled by one group of the first stage, the task shader culls 64 meshlets per group
        const uint32_t meshletsPerGroup = useAmplification ? 64u : 1u;

        PipelineStateSettings settings{};
        if (useAmplification)
        {
            settings.shaderState.amplificationShader = engine.GetShaderTracker()->GetOrCreateShader(
                "Shaders/Geometry/DeferredAmplificationShader.slang", {{"LATE_PASS", latePass ? "1" : "0"}});
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

        // Samples the HZB in the late pass amplification shader
        Uniform hzbSampler{1, 0, RootParams::RootResourceType::StaticSampler};
        hzbSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        hzbSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        hzbSampler.samplerState.addressModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        hzbSampler.visibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        uniforms.emplace_back(hzbSampler);

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

        uint32_t meshletVisibilityView = INVALID_HEAP_INDEX;
        if (m_meshletVisibilityBuffer)
        {
            m_meshletVisibilityBuffer->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            meshletVisibilityView = m_meshletVisibilityBuffer->GetUAView()->View();
        }

        // Cull against the same view projection the mesh shader reads from the scene data buffer
        const auto& visibleEntities =
            renderer.GetSystems().GetSystem<FrustumCullingSystem>().CullMeshComponents(GetSceneData().viewProjection);
        size_t nextVisible = 0;

        // Running slot in the visibility buffer
        uint32_t instanceMeshletOffset = 0;

        // Walks every mesh instead of only the visible ones so each instance keeps the same visibility buffer slots.
        // The culled list keeps the order of this view, so the next visible entity is always at nextVisible.
        auto meshes = engine.GetECS()->GetRegistry().view<Transform, MeshComponent>();
        for (auto&& [entity, trans, meshComponent] : meshes.each())
        {
            if (!meshComponent.mesh) continue;

            const bool isVisible = nextVisible < visibleEntities.size() && visibleEntities[nextVisible] == entity;
            if (isVisible) nextVisible++;

            auto& mesh = *meshComponent.mesh;
            if (!mesh.GetMeshlets()) continue;
            const auto& meshlets = mesh.GetMeshlets()->GetMeshletData();
            if (meshlets.meshletCount == 0) continue;

            const uint32_t meshletSlot = instanceMeshletOffset;
            instanceMeshletOffset += meshlets.meshletCount;
            if (!isVisible) continue;

            const glm::mat4& world = trans.GetWorldMatrix();

            DeferredMeshShaderRootConstants rc{};
            rc.instanceMeshletOffset = meshletSlot;
            rc.meshletVisibilityView = meshletVisibilityView;
            if (hzbData)
            {
                rc.hzbView = hzbData->hzbTexture->GetSrv()->View();
                rc.hzbSize = {hzbData->hzbTexture->Width(), hzbData->hzbTexture->Height()};
                rc.hzbMipCount = hzbData->hzbMipCount;
            }
            rc.model = world;
            rc.invTransposeModel = glm::mat4(glm::transpose(glm::inverse(glm::mat3(world))));
            rc.cullCameraPosition = glm::vec3(glm::inverse(world) * cameraPosition);
            const auto& material = mesh.GetMaterial();
            if (material.m_albedo) rc.albedoView = material.m_albedo->GetSrv()->BindlessView();
            if (material.m_normal) rc.normalView = material.m_normal->GetSrv()->BindlessView();
            if (material.m_roughnessMetallic)
                rc.roughnessMetallicView = material.m_roughnessMetallic->GetSrv()->BindlessView();
            if (material.m_emissive) rc.emissiveView = material.m_emissive->GetSrv()->BindlessView();
            rc.baseColor = material.baseColor;

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

                // Cull against the same view projection the vertex shader reads from the scene data buffer
                const auto& visibleEntities =
                    renderer.GetSystems().GetSystem<FrustumCullingSystem>().CullMeshComponents(GetSceneData().viewProjection);

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.albedoRoughnessTexture, passData.normalMetallicTexture, passData.emissiveTexture},
                                 {ClearOperation::Store, ClearOperation::Store, ClearOperation::Store},
                                 {passData.depthTexture},
                                 DSClearOperation::Store);

                list.SetConstantBufferView(2, GetSceneDataAddress());

                auto& registry = ecs->GetRegistry();
                for (const Entity entity : visibleEntities)
                {
                    auto [trans, meshComponent] = registry.get<Transform, MeshComponent>(entity);
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
                    m_rc.baseColor = material.baseColor;

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

        if (SupportsMeshShaderPath() && !m_forceVertexPath && m_useAmplificationShader)
        {
            DeferredEarlyPass(renderer, rg);

            // Copies the early pass depth into mip 0 of the hierarchical z buffer and builds the chain from it
            HZBDepthCopyPass(renderer, rg);
            BuildHZBPass(renderer, rg);

            DeferredLatePass(renderer, rg);
        }
        else if (SupportsMeshShaderPath() && !m_forceVertexPath)
            DeferredMeshShaderPass(renderer, rg, false);
        else
            DeferredVertexPass(renderer, rg);
    }
} // namespace Wild
