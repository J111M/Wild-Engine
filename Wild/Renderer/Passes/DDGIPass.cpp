#include "Renderer/Passes/DDGIPass.hpp"

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
        AddProbeBlendPass(renderer, rg);
    }

    void DDGIPass::Update(const float dt)
    {
        auto ecs = engine.GetECS();

        Camera* camera = GetActiveCamera();
        if (camera) m_rc.inverseView = glm::inverse(camera->GetView());

        // Use the first directional light for now
        auto view = ecs->View<DirectionalLight>();
        for (auto entity : view)
        {
            auto& directionalLight = ecs->GetComponent<DirectionalLight>(entity);
            m_rc.lightDirection = glm::vec4(directionalLight.direction, 0.0f);
            m_rc.lightColorIntensity = directionalLight.colorIntensity;
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
                m_rc.randomRotation = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);
            }
            else
            {
                m_rc.randomRotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }

        m_rc.raysPerProbe = static_cast<uint32_t>(m_raysPerProbe);
        m_rc.maxRayDistance = m_maxRayDistance;
        m_rc.intensity = m_intensity;

        m_blendRc.hysteresis = m_hysteresis;
        m_blendRc.raysPerProbe = static_cast<uint32_t>(m_raysPerProbe);

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
            std::string textureName = "Irradiance history packed data";
            desc.name = textureName;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);

            passData->radianceTexture = rg.CreateTransientTexture(textureName, desc);
        }

        {
            TextureDesc desc;
            desc.width = probeCounts.x * 16;
            desc.height = probeCounts.z * 16;
            desc.depthOrArray = probeCounts.y;
            std::string textureName = "Visibility history packed data";
            desc.name = textureName;
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);

            passData->visibilityTexture = rg.CreateTransientTexture(textureName, desc);
        }

        rg.AddPass<DDGIPassData>(
            "DDGI probe trace pass", PassType::Raytracing, [&renderer, this](DDGIPassData& passData, CommandList& list) {
                if (!enabled) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                m_rc.probeOrigin = glm::vec4(probeSystem->GetOrigin(), 0.0f);
                m_rc.probeSpacing = glm::vec4(probeSystem->GetSpacing(), 0.0f);
                m_rc.probeCounts = glm::ivec4(probeSystem->GetCounts(), static_cast<int32_t>(ProbeSystem::MAX_RAYS_PER_PROBE));

                if (renderer.environmentMap) m_rc.environmentView = renderer.environmentMap->GetSrv()->BindlessView();

                auto& lightSystem = renderer.GetSystems().GetSystem<LightSystem>();
                m_rc.numPointLights = lightSystem.GetPointLightCount();

                passData.radianceTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.visibilityTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                m_rc.irradianceView = passData.radianceTexture->GetSrv()->BindlessView();
                m_rc.distanceView = passData.visibilityTexture->GetSrv()->BindlessView();

                PipelineStateSettings settings{};

                settings.ShaderState.rayTracingShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/ddgiRaytrace.slang");

                settings.raytracingState.payloadSize = 16;
                settings.raytracingState.attributeSize = sizeof(float) * 2;

                settings.raytracingState.rayRecursionDepth = 2;

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DDGIRootConstants)};
                uniforms.emplace_back(rootConstant);

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

                list.SetRootConstant<DDGIRootConstants>(0, m_rc);
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

    void DDGIPass::AddProbeBlendPass(Renderer& renderer, RenderGraph& rg)
    {
        rg.AllocatePassData<DDGIBlendPassData>();
        rg.GetPassData<DDGIBlendPassData, DDGIPassData>();

        rg.AddPass<DDGIBlendPassData>(
            "DDGI probe blend pass", PassType::Compute, [&renderer, this](DDGIBlendPassData&, CommandList& list) {
                if (!enabled) return;

                auto* probeSystem = renderer.GetSystems().TryGetSystem<ProbeSystem>();
                if (!probeSystem || probeSystem->GetProbeCount() == 0) return;

                m_blendRc.maxRaysPerProbe = ProbeSystem::MAX_RAYS_PER_PROBE;
                m_blendRc.probeCount = probeSystem->GetProbeCount();

                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/DDGI/ddgiBlend.slang");

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(DDGIBlendRootConstants)};
                uniforms.emplace_back(rootConstant);

                Uniform rayDataBuffer{0, 0, RootParams::RootResourceType::ShaderResourceView};
                uniforms.emplace_back(rayDataBuffer);

                Uniform irradianceBuffer{0, 0, RootParams::RootResourceType::UnorderedAccessView};
                uniforms.emplace_back(irradianceBuffer);

                auto& pipeline =
                    renderer.GetOrCreatePipeline("DDGI probe blend pass", PipelineStateType::Compute, settings, uniforms);

                list.SetPipelineState(pipeline);
                list.BeginRender("DDGI probe blend pass");

                list.SetRootConstant<DDGIBlendRootConstants>(0, m_blendRc);
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
