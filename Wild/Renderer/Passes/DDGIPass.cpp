#include "Renderer/Passes/DDGIPass.hpp"

#include "Renderer/Resources/Buffer.hpp"
#include "Systems/LightSystem.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <random>

namespace Wild
{
    DDGIPass::DDGIPass() {}

    std::shared_ptr<GPUBuffer> DDGIPass::CreateConstantBuffer(size_t size, const std::string& name)
    {
        BufferDesc desc{};

        // The CBV descriptor is built from this size and D3D12 requires it to be a multiple of 256
        desc.size = (size + 255) & ~static_cast<size_t>(255);
        desc.usage = BufferUsage::Constant;
        desc.access = MemoryAccess::CpuToGpu;
        desc.name = name;

        return std::make_shared<GPUBuffer>(desc);
    }

    // TODO replace function
    uint32_t DDGIPass::GetOrCreateProbeDataView()
    {
        if (!m_probeDataBuffer) m_probeDataBuffer = CreateConstantBuffer(sizeof(DDGIProbeData), "DDGI probe data");

        auto probeDataCbv = m_probeDataBuffer->GetCBView();
        return probeDataCbv ? probeDataCbv->View() : INVALID_HEAP_INDEX;
    }

    void DDGIPass::UploadProbeData(const ProbeSystem& probeSystem, const DDGIPassData& ddgiData)
    {
        m_probeData.probeOrigin = glm::vec4(probeSystem.GetOrigin(), 0.0f);
        m_probeData.probeSpacing = glm::vec4(probeSystem.GetSpacing(), 0.0f);
        m_probeData.probeCounts = glm::ivec4(probeSystem.GetCounts(), static_cast<int32_t>(ProbeSystem::MAX_RAYS_PER_PROBE));

        const uint32_t historyIndex = ddgiData.frameParity;

        auto irradianceSrv = ddgiData.iradianceTexture[historyIndex]->GetSrv();
        auto distanceSrv = ddgiData.visibilityTexture[historyIndex]->GetSrv();

        m_probeData.irradianceView = irradianceSrv ? irradianceSrv->View() : INVALID_HEAP_INDEX;
        m_probeData.distanceView = distanceSrv ? distanceSrv->View() : INVALID_HEAP_INDEX;

        m_probeDataBuffer->Allocate(&m_probeData, sizeof(DDGIProbeData));
    }


    // TODO replace function
    void DDGIPass::FillUpdateConstants(const DDGIPassData& ddgiData, const ProbeSystem& probeSystem)
    {
        // TODO modify max ray distance
        m_updateRc.probeMaxRayDistance = glm::length(probeSystem.GetSpacing()) * 1.5f;

        const uint32_t readIndex = ddgiData.frameParity;
        const uint32_t writeIndex = ddgiData.frameParity ^ 1u;

        // Raw View() indices, the update shaders resolve them through ResourceDescriptorHeap rather than a table
        auto irradianceReadSrv = ddgiData.iradianceTexture[readIndex]->GetSrv();
        auto irradianceWriteUav = ddgiData.iradianceTexture[writeIndex]->GetUav();

        m_updateRc.irradianceReadView = irradianceReadSrv ? irradianceReadSrv->View() : INVALID_HEAP_INDEX;
        m_updateRc.irradianceWriteView = irradianceWriteUav ? irradianceWriteUav->View() : INVALID_HEAP_INDEX;

        auto distanceReadSrv = ddgiData.visibilityTexture[readIndex]->GetSrv();
        auto distanceWriteUav = ddgiData.visibilityTexture[writeIndex]->GetUav();

        m_updateRc.distanceReadView = distanceReadSrv ? distanceReadSrv->View() : INVALID_HEAP_INDEX;
        m_updateRc.distanceWriteView = distanceWriteUav ? distanceWriteUav->View() : INVALID_HEAP_INDEX;

        m_updateRc.probeDataView = ddgiData.probeDataView;
    }

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
        if (camera) m_traceRc.inverseView = glm::inverse(camera->GetView());

        // Use the first directional light for now
        auto view = ecs->View<DirectionalLight>();
        for (auto entity : view)
        {
            auto& directionalLight = ecs->GetComponent<DirectionalLight>(entity);
            m_traceRc.lightDirection = glm::vec4(directionalLight.direction, 0.0f);
            m_traceRc.lightColorIntensity = directionalLight.colorIntensity;
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
                m_probeData.randomRotation = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);
            }
            else
            {
                m_probeData.randomRotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }

        m_probeData.raysPerProbe = static_cast<uint32_t>(m_raysPerProbe);
        m_probeData.maxRayDistance = m_maxRayDistance;
        m_probeData.intensity = m_intensity;
        m_probeData.hysteresis = m_hysteresis;
        m_probeData.normalBias = m_normalBias;
        m_probeData.viewBias = m_viewBias;

        if (enabled) m_frameParity ^= 1u;

        engine.GetImGui()->AddPanel("DDGI Settings", [this]() {
            ImGui::Checkbox("Enabled", &enabled);
            ImGui::SliderInt("Rays Per Probe", &m_raysPerProbe, 1, static_cast<int>(ProbeSystem::MAX_RAYS_PER_PROBE));
            ImGui::SliderFloat("Hysteresis", &m_hysteresis, 0.0f, 0.99f);
            ImGui::SliderFloat("Max Ray Distance", &m_maxRayDistance, 1.0f, 5000.0f);
            ImGui::SliderFloat("Intensity", &m_intensity, 0.0f, 5.0f);
            ImGui::SliderFloat("Normal Bias", &m_normalBias, 0.0f, 0.5f);
            ImGui::SliderFloat("View Bias", &m_viewBias, 0.0f, 0.5f);
            ImGui::Checkbox("Random Ray Rotation", &m_randomRayRotation);
            ImGui::Checkbox("Freeze Updates", &m_freezeUpdates);
        });
    }

    void DDGIPass::AddProbeTracePass(Renderer& renderer, RenderGraph& rg)
    {
        DDGIPassData* passData = rg.AllocatePassData<DDGIPassData>();

        passData->frameParity = m_frameParity;

        passData->probeDataView = enabled ? GetOrCreateProbeDataView() : INVALID_HEAP_INDEX;

        auto probeSystem = renderer.GetSystems().GetSystem<ProbeSystem>();

        // if (!probeSystem) WD_FATAL("Probe system doesn't exist");
        //  Use max probe count instead of current probe
        const glm::ivec3 probeCounts = probeSystem.GetCounts();

        {
            TextureDesc desc;
            desc.width = probeCounts.x * 8;
            desc.height = probeCounts.z * 8;
            desc.depthOrArray = probeCounts.y;

            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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
            desc.format = DXGI_FORMAT_R32G32_FLOAT;
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

                // SampleDDGIIrradiance resolves both probe atlases straight from the descriptor heap
                if (!engine.GetGfxContext()->GetCapabilities().CheckResourceBindingSupport(ResourceBindingSupport::Tier3))
                {
                    static bool bindingTierWarned = false;
                    if (!bindingTierWarned)
                    {
                        WD_WARN("DDGI probe trace pass needs resource binding tier 3, skipping.");
                        bindingTierWarned = true;
                    }
                    return;
                }

                if (passData.probeDataView == INVALID_HEAP_INDEX)
                {
                    static bool probeDataWarned = false;
                    if (!probeDataWarned)
                    {
                        WD_WARN("DDGI probe data constant buffer has no descriptor heap slot, skipping the trace pass.");
                        probeDataWarned = true;
                    }
                    return;
                }

                if (renderer.environmentMap) m_traceRc.environmentView = renderer.environmentMap->GetSrv()->BindlessView();

                auto& lightSystem = renderer.GetSystems().GetSystem<LightSystem>();
                m_traceRc.numPointLights = lightSystem.GetPointLightCount();

                // Sample the half the update passes wrote last frame, which is the half they read this frame. The
                // other half is only ever a UAV within a frame, so the two states never fight over one texture.
                const uint32_t historyIndex = passData.frameParity;

                passData.iradianceTexture[historyIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.visibilityTexture[historyIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                UploadProbeData(*probeSystem, passData);

                m_traceRc.probeDataView = passData.probeDataView;

                if (!m_traceConstantBuffer)
                {
                    m_traceConstantBuffer = CreateConstantBuffer(sizeof(DDGITraceConstants), "DDGI trace constants");
                }

                m_traceConstantBuffer->Allocate(&m_traceRc, sizeof(DDGITraceConstants));

                PipelineStateSettings settings{};

                settings.ShaderState.rayTracingShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIRaytrace.slang");

                settings.raytracingState.payloadSize = 16;
                settings.raytracingState.attributeSize = sizeof(float) * 2;

                settings.raytracingState.rayRecursionDepth = 2;

                std::vector<Uniform> uniforms;

                Uniform traceConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(traceConstants);

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

                Uniform clampSampler{1, 0, RootParams::RootResourceType::StaticSampler};
                clampSampler.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                clampSampler.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                clampSampler.samplerState.addressModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                uniforms.emplace_back(clampSampler);

                auto& pipeline = renderer.GetOrCreatePipeline(
                    "Dynamic Diffuse Global Illumination Pass", PipelineStateType::Raytracing, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI probe trace pass");

                list.SetConstantBufferView(0, m_traceConstantBuffer.get());
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

                FillUpdateConstants(*ddgiData, *probeSystem);

                // An invalid index means the texture had no usable view, never fall back to heap slot 0 since
                // nothing is allocated there.
                if (m_updateRc.irradianceReadView == INVALID_HEAP_INDEX || m_updateRc.irradianceWriteView == INVALID_HEAP_INDEX)
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

                if (!m_irradianceConstantBuffer)
                {
                    m_irradianceConstantBuffer =
                        CreateConstantBuffer(sizeof(DDGIUpdateConstants), "DDGI update irradiance constants");
                }

                m_irradianceConstantBuffer->Allocate(&m_updateRc, sizeof(DDGIUpdateConstants));

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIUpdateIrradiance.slang");

                std::vector<Uniform> uniforms;

                Uniform updateConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(updateConstants);

                Uniform rayDataBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(rayDataBuffer);

                Uniform irradianceBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(irradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("DDGI update irradiance pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI update irradiance pass");

                list.SetConstantBufferView(0, m_irradianceConstantBuffer.get());
                list.SetShaderResourceView(1, probeSystem->GetProbeRayDataBuffer().get());
                list.SetUnorderedAccessView(2, probeSystem->GetProbeIrradianceBuffer().get());

                const glm::ivec3 counts = probeSystem->GetCounts();
                list.GetList()->Dispatch(
                    static_cast<uint32_t>(counts.x), static_cast<uint32_t>(counts.z), static_cast<uint32_t>(counts.y));

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = probeSystem->GetProbeIrradianceBuffer()->GetBuffer();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }

    void DDGIPass::AddUpdateDistancePass(Renderer& renderer, RenderGraph& rg)
    {
        rg.AllocatePassData<UpdateDistancePassData>();
        DDGIPassData* ddgiData = rg.GetPassData<UpdateDistancePassData, DDGIPassData>();

        rg.AddPass<UpdateDistancePassData>(
            "DDGI update distance pass",
            PassType::Compute,
            [&renderer, ddgiData, this](UpdateDistancePassData&, CommandList& list) {
                if (!enabled) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                // The shader resolves both textures straight from the descriptor heap
                if (!engine.GetGfxContext()->GetCapabilities().CheckResourceBindingSupport(ResourceBindingSupport::Tier3))
                {
                    static bool bindingTierWarned = false;
                    if (!bindingTierWarned)
                    {
                        WD_WARN("DDGI update distance pass needs resource binding tier 3, skipping.");
                        bindingTierWarned = true;
                    }
                    return;
                }

                FillUpdateConstants(*ddgiData, *probeSystem);

                // An invalid index means the texture had no usable view, never fall back to heap slot 0 since
                // nothing is allocated there.
                if (m_updateRc.distanceReadView == INVALID_HEAP_INDEX || m_updateRc.distanceWriteView == INVALID_HEAP_INDEX)
                {
                    static bool viewsWarned = false;
                    if (!viewsWarned)
                    {
                        WD_WARN("DDGI distance textures have no usable SRV / UAV pair, skipping the update pass.");
                        viewsWarned = true;
                    }
                    return;
                }

                const uint32_t readIndex = ddgiData->frameParity;
                const uint32_t writeIndex = ddgiData->frameParity ^ 1u;

                ddgiData->visibilityTexture[readIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                ddgiData->visibilityTexture[writeIndex]->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                if (!m_distanceConstantBuffer)
                {
                    m_distanceConstantBuffer =
                        CreateConstantBuffer(sizeof(DDGIUpdateConstants), "DDGI update distance constants");
                }

                m_distanceConstantBuffer->Allocate(&m_updateRc, sizeof(DDGIUpdateConstants));

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/DDGIUpdateDistance.slang");

                std::vector<Uniform> uniforms;

                Uniform updateConstants{0, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(updateConstants);

                Uniform rayDataBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(rayDataBuffer);

                Uniform irradianceBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(irradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("DDGI update distance pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI update distance pass");

                list.SetConstantBufferView(0, m_distanceConstantBuffer.get());
                list.SetShaderResourceView(1, probeSystem->GetProbeRayDataBuffer().get());
                list.SetUnorderedAccessView(2, probeSystem->GetProbeIrradianceBuffer().get());

                const glm::ivec3 counts = probeSystem->GetCounts();
                list.GetList()->Dispatch(
                    static_cast<uint32_t>(counts.x), static_cast<uint32_t>(counts.z), static_cast<uint32_t>(counts.y));

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = probeSystem->GetProbeIrradianceBuffer()->GetBuffer();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }
} // namespace Wild
