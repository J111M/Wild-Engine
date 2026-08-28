#include "Renderer/Passes/DDGIPass.hpp"

#include "Renderer/Resources/Buffer.hpp"
#include "Systems/LightSystem.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <random>

namespace Wild
{
    DDGIPass::DDGIPass() {}

    void DDGIPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        AddProbeTracePass(renderer, rg);
        AddUpdateIrradiancePass(renderer, rg);
        AddUpdateDistancePass(renderer, rg);
    }

    void DDGIPass::Update(const float dt)
    {
        auto ecs = engine.GetECS();

        Camera* camera = GetActiveCamera();
        if (camera) m_ddgiRc.inverseView = glm::inverse(camera->GetView());

        // Use the first directional light for now
        auto view = ecs->View<DirectionalLight>();
        for (auto entity : view)
        {
            auto& directionalLight = ecs->GetComponent<DirectionalLight>(entity);
            m_ddgiRc.lightDirection = glm::vec4(directionalLight.direction, 0.0f);
            m_ddgiRc.lightColorIntensity = directionalLight.colorIntensity;
            break;
        }

        if (!m_freezeUpdates)
        {
            if (m_randomRayRotation)
            {
                static std::mt19937 rng{std::random_device{}()};
                static std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
                static std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);

                glm::vec3 axis = glm::normalize(glm::vec3(axisDist(rng), axisDist(rng), axisDist(rng)));
                glm::quat rotation = glm::angleAxis(angleDist(rng), axis);
                m_ddgiRc.randomRotation = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);
            }
            else { m_ddgiRc.randomRotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); }
        }

        m_ddgiRc.raysPerProbe = static_cast<uint32_t>(m_raysPerProbe);
        m_ddgiRc.maxRayDistance = m_maxRayDistance;
        m_ddgiRc.intensity = m_intensity;
        m_ddgiRc.hysteresis = m_hysteresis;

        engine.GetImGui()->AddPanel("DDGI Settings", [this]() {
            ImGui::Checkbox("Enabled", &enabled);
            ImGui::SliderInt("Rays Per Probe", &m_raysPerProbe, 1, static_cast<int>(ProbeSystem::MAX_RAYS_PER_PROBE));
            ImGui::SliderFloat("Hysteresis", &m_hysteresis, 0.0f, 0.99f);
            ImGui::SliderFloat("Max Ray Distance", &m_maxRayDistance, 1.0f, 5000.0f);
            ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 5.0f);
            ImGui::Checkbox("Random Ray Rotation", &m_randomRayRotation);
            ImGui::Checkbox("Freeze Updates", &m_freezeUpdates);
        });
    }

    void DDGIPass::AddProbeTracePass(Renderer& renderer, RenderGraph& rg)
    {
        DDGIPassData* passData = rg.AllocatePassData<DDGIPassData>();

        auto probeSystem = renderer.GetSystems().GetSystem<ProbeSystem>();

        // if (!probeSystem) WD_FATAL("Probe system doesn't exist");
        //  Use max probe count instead of current probe
        const glm::ivec3 probeCounts = probeSystem.GetCounts();

        {
            TextureDesc desc;
            desc.width = probeCounts.x * 8;
            desc.height = probeCounts.z * 8;
            desc.depthOrArray = probeCounts.y;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);

            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                std::string textureName = "Irradiance history buffer: " + std::to_string(i) + " packed data";
                desc.name = textureName;

                passData->iradianceTexture[i] = rg.CreateTransientTexture(textureName, desc);
            }
        }

        {
            TextureDesc desc;
            desc.width = probeCounts.x * 16;
            desc.height = probeCounts.z * 16;
            desc.depthOrArray = probeCounts.y;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);

            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                std::string textureName = "Visibility history buffer: " + std::to_string(i) + " packed data";
                desc.name = textureName;

                passData->visibilityTexture[i] = rg.CreateTransientTexture(textureName, desc);
            }
        }

        rg.AddPass<DDGIPassData>(
            "DDGI probe trace pass", PassType::Raytracing, [&renderer, this](DDGIPassData& passData, CommandList& list) {
                if (!enabled) return;

                // Change to GetSystem instead of TryGetSystem to ensure the probe system exists
                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                m_ddgiRc.probeOrigin = glm::vec4(probeSystem->GetOrigin(), 0.0f);
                m_ddgiRc.probeSpacing = glm::vec4(probeSystem->GetSpacing(), 0.0f);
                m_ddgiRc.probeCounts =
                    glm::ivec4(probeSystem->GetCounts(), static_cast<int32_t>(ProbeSystem::MAX_RAYS_PER_PROBE));

                if (renderer.environmentMap) m_ddgiRc.environmentView = renderer.environmentMap->GetSrv()->BindlessView();

                auto& lightSystem = renderer.GetSystems().GetSystem<LightSystem>();
                m_ddgiRc.numPointLights = lightSystem.GetPointLightCount();

                // TODO 1 texture for reading and the other for read write
                passData.iradianceTexture[0]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.visibilityTexture[0]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                m_ddgiRc.irradianceView = passData.iradianceTexture[0]->GetSrv()->BindlessView();
                m_ddgiRc.distanceView = passData.visibilityTexture[0]->GetSrv()->BindlessView();

                // Heap slots of the ping-pong pair the update pass runs on. Filled in here because this is the
                // first DDGI pass to execute and the only one holding the pass data. Raw View() indices, the
                // update shader resolves them through ResourceDescriptorHeap rather than a descriptor table.
                {
                    const uint32_t readIndex = passData.frameParity;
                    const uint32_t writeIndex = passData.frameParity ^ 1u;

                    auto readSrv = passData.iradianceTexture[readIndex]->GetSrv();
                    auto writeUav = passData.iradianceTexture[writeIndex]->GetUav();

                    m_ddgiRc.irradianceReadView = readSrv ? readSrv->View() : INVALID_HEAP_INDEX;
                    m_ddgiRc.irradianceWriteView = writeUav ? writeUav->View() : INVALID_HEAP_INDEX;
                }

                // Every DDGI pass reads these constants from the same buffer and they all record into one command
                // list, so the GPU only ever observes the last CPU write. It is filled and uploaded once here,
                // in the pass the graph runs first, and the update passes only bind it.
                if (!m_ddgiConstantBuffer)
                {
                    BufferDesc constantsDesc{};
                    constantsDesc.size = sizeof(DDGIRootConstants);
                    constantsDesc.usage = BufferUsage::Constant;
                    constantsDesc.access = MemoryAccess::CpuToGpu;
                    constantsDesc.name = "DDGI constants";

                    m_ddgiConstantBuffer = std::make_shared<GPUBuffer>(constantsDesc);
                }

                m_ddgiConstantBuffer->Allocate(&m_ddgiRc, sizeof(DDGIRootConstants));

                PipelineStateSettings settings{};

                settings.ShaderState.rayTracingShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIRaytrace.slang");

                settings.raytracingState.payloadSize = 16;
                settings.raytracingState.attributeSize = sizeof(float) * 2;

                settings.raytracingState.rayRecursionDepth = 2;

                std::vector<Uniform> uniforms;

                Uniform ddgiConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(ddgiConstants);

                Uniform accelerationStructure{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(accelerationStructure);

                Uniform meshInfoBuffer{1, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(meshInfoBuffer);

                Uniform probeBuffer{2, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(probeBuffer);

                Uniform probeIrradianceBuffer{3, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(probeIrradianceBuffer);

                Uniform probeRayDataBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(probeRayDataBuffer);

                Uniform bindlessHeap{0, 1, RootParams::RootResourceType::DescriptorTable};
                CD3DX12_DESCRIPTOR_RANGE bufRange{};
                bufRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                CD3DX12_DESCRIPTOR_RANGE texRange{};
                texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 2, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                CD3DX12_DESCRIPTOR_RANGE cubeTexRange{};
                cubeTexRange.Init(
                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 3, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
                bindlessHeap.ranges.emplace_back(bufRange);
                bindlessHeap.ranges.emplace_back(texRange);
                bindlessHeap.ranges.emplace_back(cubeTexRange);
                uniforms.emplace_back(bindlessHeap);

                Uniform pointLightBuffer{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(pointLightBuffer);

                Uniform staticSampler{0, 0, RootParams::RootResourceType::StaticSampler};
                uniforms.emplace_back(staticSampler);


                auto& pipeline = renderer.GetOrCreatePipeline(
                    "Dynamic Diffuse Global Illumination Pass", PipelineStateType::Raytracing, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI probe trace pass");

                list.SetConstantBufferView(0, m_ddgiConstantBuffer.get());
                list.GetList()->SetComputeRootShaderResourceView(1, engine.GetAccelerationStructureManager()->GetTLASAddress());
                list.SetShaderResourceView(2, engine.GetAccelerationStructureManager()->GetMeshIdBuffer().get());
                list.SetShaderResourceView(3, probeSystem->GetProbeBuffer().get());
                list.SetShaderResourceView(4, probeSystem->GetProbeIrradianceBuffer().get());
                list.SetUnorderedAccessView(5, probeSystem->GetProbeRayDataBuffer().get());
                list.SetBindlessHeap(6);
                list.SetConstantBufferView(7, lightSystem.GetPointLightBuffer().get());

                list.Dispatch(m_raysPerProbe, static_cast<uint32_t>(probeSystem->GetProbeCount()), 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = probeSystem->GetProbeRayDataBuffer()->GetBuffer();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }

    /// <summary>
    /// Updates the irradiances texture with the new traced data accumulating the results
    /// </summary>
    /// <param name="renderer">Reference to the renderer used to obtain systems, pipelines and shaders.</param>
    /// <param name="rg">Render graph instance to which the compute pass will be added.</param>
    void DDGIPass::AddUpdateIrradiancePass(Renderer& renderer, RenderGraph& rg)
    {
        rg.AllocatePassData<UpdateIrradiancePassData>();
        DDGIPassData* ddgiData = rg.GetPassData<UpdateIrradiancePassData, DDGIPassData>();

        rg.AddPass<UpdateIrradiancePassData>(
            "DDGI update irradiance pass",
            PassType::Compute,
            [&renderer, ddgiData, this](UpdateIrradiancePassData&, CommandList& list) {
                if (!enabled) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                // The shader resolves both textures straight from the descriptor heap
                if (!engine.GetGfxContext()->GetCapabilities().CheckResourceBindingSupport(ResourceBindingSupport::Tier3))
                {
                    static bool bindingTierWarned = false;
                    if (!bindingTierWarned)
                    {
                        WD_WARN("DDGI update irradiance pass needs resource binding tier 3, skipping.");
                        bindingTierWarned = true;
                    }
                    return;
                }

                // The trace pass fills these in and must have run to upload the constants. An invalid index means
                // the texture had no usable view, never fall back to heap slot 0 since nothing is allocated there.
                if (!m_ddgiConstantBuffer || m_ddgiRc.irradianceReadView == INVALID_HEAP_INDEX ||
                    m_ddgiRc.irradianceWriteView == INVALID_HEAP_INDEX)
                {
                    static bool viewsWarned = false;
                    if (!viewsWarned)
                    {
                        WD_WARN("DDGI irradiance textures have no usable SRV / UAV pair, skipping the update pass.");
                        viewsWarned = true;
                    }
                    return;
                }

                const uint32_t readIndex = ddgiData->frameParity;
                const uint32_t writeIndex = ddgiData->frameParity ^ 1u;

                ddgiData->iradianceTexture[readIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                ddgiData->iradianceTexture[writeIndex]->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                // Kept in sync with the trace pass, which is the pass that uploads the shared constant buffer
                m_ddgiRc.probeCounts =
                    glm::ivec4(probeSystem->GetCounts(), static_cast<int32_t>(ProbeSystem::MAX_RAYS_PER_PROBE));

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIUpdateIrradiance.slang");

                std::vector<Uniform> uniforms;

                Uniform ddgiConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(ddgiConstants);

                Uniform rayDataBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(rayDataBuffer);

                Uniform irradianceBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(irradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("DDGI update irradiance pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI update irradiance pass");

                list.SetConstantBufferView(0, m_ddgiConstantBuffer.get());
                list.SetShaderResourceView(1, probeSystem->GetProbeRayDataBuffer().get());
                list.SetUnorderedAccessView(2, probeSystem->GetProbeIrradianceBuffer().get());

                uint32_t groupCount = (probeSystem->GetProbeCount() + 63) / 64;
                list.GetList()->Dispatch(groupCount, 1, 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = probeSystem->GetProbeIrradianceBuffer()->GetBuffer();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }

    void DDGIPass::AddUpdateDistancePass(Renderer& renderer, RenderGraph& rg)
    {
        rg.AllocatePassData<UpdateIrradiancePassData>();
        rg.GetPassData<UpdateIrradiancePassData, DDGIPassData>();

        rg.AddPass<UpdateIrradiancePassData>(
            "DDGI update distance pass", PassType::Compute, [&renderer, this](UpdateIrradiancePassData&, CommandList& list) {
                if (!enabled) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                // Kept in sync with the trace pass, which is the pass that uploads the shared constant buffer
                m_ddgiRc.probeCounts =
                    glm::ivec4(probeSystem->GetCounts(), static_cast<int32_t>(ProbeSystem::MAX_RAYS_PER_PROBE));

                // Filled and uploaded by the trace pass, which the graph orders ahead of this one
                if (!m_ddgiConstantBuffer) return;

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIUpdateDistance.slang");

                std::vector<Uniform> uniforms;

                Uniform ddgiConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(ddgiConstants);

                Uniform rayDataBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(rayDataBuffer);

                Uniform irradianceBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(irradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("DDGI update distance pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI update distance pass");

                list.SetConstantBufferView(0, m_ddgiConstantBuffer.get());
                list.SetShaderResourceView(1, probeSystem->GetProbeRayDataBuffer().get());
                list.SetUnorderedAccessView(2, probeSystem->GetProbeIrradianceBuffer().get());

                uint32_t groupCount = (probeSystem->GetProbeCount() + 63) / 64;
                list.GetList()->Dispatch(groupCount, 1, 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = probeSystem->GetProbeIrradianceBuffer()->GetBuffer();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }
} // namespace Wild
