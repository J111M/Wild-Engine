#include "Renderer/Passes/OceanPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"

#include "Renderer/Resources/Mesh.hpp"

#include <random>

namespace Wild
{
    OceanPass::OceanPass()
    {
        // Generate the Gaussian noise
        std::mt19937 rng(0);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        std::vector<ComplexValue> val(OCEAN_SIZE * OCEAN_SIZE);

        for (uint32_t i = 0; i < OCEAN_SIZE * OCEAN_SIZE; ++i)
        {
            auto [re, im] = BoxMuller(dist(rng), dist(rng));
            val[i] = {re, im};
        }

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(ComplexValue);
            desc.numOfElements = val.size();
            m_gaussianDistribution = std::make_unique<Buffer>(desc, uav);
            m_gaussianDistribution->UploadToGPU(val.data());
        }

        GenerateOceanPlane(256);
        m_chunkEntity = engine.GetECS()->CreateEntity();
        auto& transform = engine.GetECS()->AddComponent<Transform>(m_chunkEntity, glm::vec3(0, 0, 0), m_chunkEntity);
        transform.SetPosition(glm::vec3(0, -9, 0));

        {
            BufferDesc desc{};
            desc.bufferSize = sizeof(CameraBuffer);
            m_cameraBuffer = std::make_unique<Buffer>(desc, BufferType::constant);
        }
    }

    void OceanPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        CalculateIntitialSpectrum(renderer, rg);
        ConjugateSpectrumPass(renderer, rg);
        UpdateSpectrum(renderer, rg);
        FastFourierPass(renderer, rg);
        AssembleOceanPass(renderer, rg);
        AddDrawOceanPass(renderer, rg);
    }

    void OceanPass::Update(const float dt)
    {
        engine.GetImGui()->AddPanel("Ocean Settings", [this]() {
            if (ImGui::CollapsingHeader("Spectrum settings"))
            {
                for (size_t i = 0; i < 2; i++)
                {
                    ImGui::PushID(i);
                    ImGui::Text("Spectrum %d", i);
                    ImGui::SliderFloat("Scale", &m_initialSpectrumRC.m_spectrumSettings[i].scale, 0.0f, 500);
                    ImGui::SliderFloat("Angle", &m_initialSpectrumRC.m_spectrumSettings[i].angle, -3.14159f, 3.14159f);
                    ImGui::SliderFloat("Spread Blend", &m_initialSpectrumRC.m_spectrumSettings[i].spreadBlend, 0.0f, 1.0f);
                    ImGui::SliderFloat("Swell", &m_initialSpectrumRC.m_spectrumSettings[i].swell, 0.01f, 1.0f);
                    ImGui::SliderFloat("Gamma", &m_initialSpectrumRC.m_spectrumSettings[i].gamma, 0.0f, 10.0f);
                    ImGui::SliderFloat("Short Waves Fade", &m_initialSpectrumRC.m_spectrumSettings[i].shortWavesFade, 0.0f, 1.0f);
                    ImGui::SliderFloat("Alpha", &m_initialSpectrumRC.m_spectrumSettings[i].alpha, 0.0f, 0.5);
                    ImGui::SliderFloat("Peak omega", &m_initialSpectrumRC.m_spectrumSettings[i].peakOmega, 0.0f, 10.0f);
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }

            if (ImGui::CollapsingHeader("Foam settings"))
            {
                ImGui::SliderFloat("Foam Bias", &m_assembleRC.foamBias, 1.4f, 1.6f);
                ImGui::SliderFloat("Foam Decay Rate", &m_assembleRC.foamDecayRate, 0.0f, 1.0f);
                ImGui::SliderFloat("Foam Threshold", &m_assembleRC.foamThreshold, 0.0f, 1.0f);
                ImGui::SliderFloat("Foam Add", &m_assembleRC.foamAdd, 0.0f, 2.0f);
            }

            if (ImGui::CollapsingHeader("Water Settings"))
            {
                ImGui::SliderFloat("Normal Scalar", &m_oceanRC.normalScalar, 0.0f, 30.0f);
                ImGui::SliderFloat("Reflection Scalar", &m_oceanRC.reflectionScalar, 0.0f, 1.0f);
                ImGui::SliderFloat("Wave Height Scalar", &m_oceanRC.waveHeightScalar, 0.0f, 5.0f);
                ImGui::SliderFloat("Air Bubble Density", &m_oceanRC.airBubbleDensity, 0.0f, 5.0f);
            }

            if (ImGui::CollapsingHeader("Color Settings"))
            {
                ImGui::ColorEdit4("Water Scatter Color", &m_oceanRC.waterScatterColor[0]);
                ImGui::ColorEdit4("Air Bubbles Color", &m_oceanRC.airBubblesColor[0]);
                ImGui::ColorEdit4("Foam Color", &m_oceanRC.foamColor[0]);
            }

            if (ImGui::CollapsingHeader("Scatter Settings"))
            {
                ImGui::SliderFloat("Peak Scatter Strength", &m_oceanRC.peakScatterStrength, 0.0f, 5.0f);
                ImGui::SliderFloat("Scatter Strength", &m_oceanRC.scatterStrength, 0.0f, 5.0f);
                ImGui::SliderFloat("Scatter Shadow Strength", &m_oceanRC.scatterShadowStrength, 0.0f, 5.0f);
            }

            ImGui::SliderFloat("Ocean depth", &m_initialSpectrumRC.oceanDepth, 0.0f, 100.0f);

            ImGui::Separator();
            for (size_t i = 0; i < 4; i++)
            {
                std::string name = "OceanScalar " + std::to_string(i);
                ImGui::SliderFloat(name.c_str(), &m_oceanRC.uvScalars[i], 0.0f, 10.0f, "%.01f");
            }
            ImGui::Separator();

            static bool updateSpectrum = false;
            ImGui::Checkbox("Recompute Initial spectrum", &updateSpectrum);
            if (updateSpectrum) m_recomputeInitialSpectrum = true;
        });

        m_updateSpectrumRC.time += 1.0 * dt;
    }

    void OceanPass::CalculateIntitialSpectrum(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<InitialSpectrumPassData>();

        if (OCEAN_CASCADES > 4) { WD_FATAL("Engine does not support more than 4 ocean cascades."); }

        TextureDesc desc;
        desc.width = OCEAN_SIZE;
        desc.height = OCEAN_SIZE;
        desc.depthOrArray = OCEAN_CASCADES;
        desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.type = TextureType::TEXTURE_3D;
        desc.name = "InitialSpectrumTexture";
        desc.usage = TextureDesc::gpuOnly;
        desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
        desc.shouldStayInCache = true;
        passData->initialSpectrumTexture = rg.CreateTransientTexture("InitialSpectrumTexture", desc);

        rg.AddPass<InitialSpectrumPassData>(
            "Initial spectrum ocean pass",
            PassType::Compute,
            [&renderer, desc, this](InitialSpectrumPassData& passData, CommandList& list) {
                if (m_recomputeInitialSpectrum)
                {
                    m_gaussianDistribution->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    PipelineStateSettings settings{};
                    settings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Ocean/InitialSpectrum.slang");

                    std::vector<Uniform> uniforms;

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(InitialSpectrumRC)};
                    uniforms.emplace_back(rootConstant);

                    Uniform gaussianNumbers{0, 0, RootParams::RootResourceType::ShaderResourceView};
                    uniforms.emplace_back(gaussianNumbers);

                    Uniform initialSpectrumUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    initialSpectrumUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(initialSpectrumUAV);

                    //// 2 spectrum settings
                    // for (size_t i = 0; i < 2; i++)
                    //{
                    //     m_initialSpectrumRC.m_spectrumSettings[i].alpha =
                    //         0.076f * glm::pow(GRAVITY * m_fetch[i] / m_windSpeed[i] / m_windSpeed[i], -0.22f);

                    //    float fetch = GRAVITY * m_fetch[i] / (m_windSpeed[i] * m_windSpeed[i]);
                    //    m_initialSpectrumRC.m_spectrumSettings[i].peakOmega =
                    //        22.0f * (GRAVITY / m_windSpeed[i]) * pow(fetch, -0.33f);
                    //}

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Initial spectrum ocean pass", PipelineStateType::Compute, settings, uniforms);
                    list.SetPipelineState(pipeline);
                    list.BeginRender("Initial spectrum ocean pass");

                    list.SetRootConstant<InitialSpectrumRC>(0, m_initialSpectrumRC);
                    list.SetShaderResourceView(1, m_gaussianDistribution.get());
                    list.SetUnorderedAccessView(2, passData.initialSpectrumTexture);

                    list.GetList()->Dispatch(desc.width / 8, desc.height / 8, 1);

                    list.EndRender();

                    D3D12_RESOURCE_BARRIER uavBarrier = {};
                    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    uavBarrier.UAV.pResource = passData.initialSpectrumTexture->GetResource();

                    list.GetList()->ResourceBarrier(1, &uavBarrier);
                }
            });
    }

    void OceanPass::ConjugateSpectrumPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<ConjugatePassData>();
        auto* initialSpectrumData = rg.GetPassData<ConjugatePassData, InitialSpectrumPassData>();

        passData->conjugatedTexture = initialSpectrumData->initialSpectrumTexture;

        rg.AddPass<ConjugatePassData>(
            "Conjugate spectrum ocean pass",
            PassType::Compute,
            [&renderer, this](ConjugatePassData& passData, CommandList& list) {
                if (m_recomputeInitialSpectrum)
                {
                    PipelineStateSettings settings{};
                    settings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/Ocean/ConjugateSpectrum.slang");

                    std::vector<Uniform> uniforms;

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(InitialSpectrumRC)};
                    uniforms.emplace_back(rootConstant);

                    Uniform conjugateSpectrumUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    conjugateSpectrumUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(conjugateSpectrumUAV);

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Conjugate spectrum ocean pass", PipelineStateType::Compute, settings, uniforms);
                    list.SetPipelineState(pipeline);
                    list.BeginRender("Conjugate spectrum ocean pass");

                    list.SetRootConstant<InitialSpectrumRC>(0, m_initialSpectrumRC);
                    list.SetUnorderedAccessView(1, passData.conjugatedTexture);

                    list.GetList()->Dispatch(OCEAN_SIZE / 8, OCEAN_SIZE / 8, 1);

                    list.EndRender();

                    passData.conjugatedTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    m_recomputeInitialSpectrum = false;
                }
            });
    }

    void OceanPass::UpdateSpectrum(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<UpdateSpectrumPassData>();
        auto* conjugateSpectrumData = rg.GetPassData<UpdateSpectrumPassData, ConjugatePassData>();

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.depthOrArray = OCEAN_CASCADES * 2;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.type = TextureType::TEXTURE_3D;
            desc.name = "Spectrum Texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->spectrumTexture = rg.CreateTransientTexture("Spectrum Texture", desc);
        }

        rg.AddPass<UpdateSpectrumPassData>(
            "Update spectrum ocean pass",
            PassType::Compute,
            [&renderer, conjugateSpectrumData, this](UpdateSpectrumPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Ocean/UpdateSpectrum.slang");

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(UpdateSpectrumRC)};
                uniforms.emplace_back(rootConstant);

                {
                    Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE srvRange{};
                    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  UINT_MAX,
                                  0,
                                  0,
                                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                    uni.ranges.emplace_back(srvRange);
                    uniforms.emplace_back(uni);
                }

                {
                    Uniform displacementUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    displacementUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(displacementUAV);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Update spectrum ocean pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Update spectrum ocean pass");

                m_updateSpectrumRC.h0TextureView = conjugateSpectrumData->conjugatedTexture->GetSrv()->BindlessView();

                list.SetRootConstant<UpdateSpectrumRC>(0, m_updateSpectrumRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.spectrumTexture);

                list.GetList()->Dispatch(OCEAN_SIZE / 8, OCEAN_SIZE / 8, 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = passData.spectrumTexture->GetResource();

                list.GetList()->ResourceBarrier(1, &uavBarrier);
            });
    }

    void OceanPass::FastFourierPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<IFFTPassData>();
        auto* spectrumData = rg.GetPassData<IFFTPassData, UpdateSpectrumPassData>();

        passData->fourierTarget = spectrumData->spectrumTexture;

        rg.AddPass<IFFTPassData>(
            "Inverse FFT ocean pass",
            PassType::Compute,
            [&renderer, spectrumData, this](IFFTPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Ocean/InverseFFT.slang");

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(IFFTRC)};
                uniforms.emplace_back(rootConstant);

                {
                    Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE srvRange{};
                    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  UINT_MAX,
                                  0,
                                  0,
                                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                    uni.ranges.emplace_back(srvRange);
                    uniforms.emplace_back(uni);
                }

                {
                    Uniform fourierTargetUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    fourierTargetUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(fourierTargetUAV);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Inverse FFT ocean pass", PipelineStateType::Compute, settings, uniforms);

                // Horizontal FFT
                m_ifftRC.axisFlag = 0;
                // m_ifftRC.spectrumTextureView = spectrumData->spectrumTexture->GetSrv()->BindlessView();

                list.SetPipelineState(pipeline);
                list.BeginRender("Horizontal inverse FFT ocean pass");

                list.SetRootConstant<IFFTRC>(0, m_ifftRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.fourierTarget);

                list.GetList()->Dispatch(1, OCEAN_SIZE, 1);

                list.EndRender();

                D3D12_RESOURCE_BARRIER uavBarrier = {};
                uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                uavBarrier.UAV.pResource = passData.fourierTarget->GetResource();

                list.GetList()->ResourceBarrier(1, &uavBarrier);

                // Vertical pass
                m_ifftRC.axisFlag = 1;

                list.SetPipelineState(pipeline);
                list.BeginRender("Vertical inverse FFT ocean pass");

                list.SetRootConstant<IFFTRC>(0, m_ifftRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.fourierTarget);

                list.GetList()->Dispatch(1, OCEAN_SIZE, 1);
                list.EndRender();

                passData.fourierTarget->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            });
    }

    void OceanPass::AssembleOceanPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<AssembleOceanPassData>();
        auto* fourierData = rg.GetPassData<AssembleOceanPassData, IFFTPassData>();

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.depthOrArray = OCEAN_CASCADES;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.type = TextureType::TEXTURE_3D;
            desc.name = "Displacement texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->displacementTexture = rg.CreateTransientTexture("Displacement texture", desc);
        }

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.depthOrArray = OCEAN_CASCADES;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.type = TextureType::TEXTURE_3D;
            desc.name = "Slope texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->slopeTexture = rg.CreateTransientTexture("Slope texture", desc);
        }

        rg.AddPass<AssembleOceanPassData>(
            "Assemble ocean pass",
            PassType::Compute,
            [&renderer, fourierData, this](AssembleOceanPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.ComputeShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/Ocean/AssembleOcean.slang");

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(AssembleOceanRC)};
                uniforms.emplace_back(rootConstant);

                {
                    Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE srvRange{};
                    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  UINT_MAX,
                                  0,
                                  0,
                                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                    uni.ranges.emplace_back(srvRange);
                    uniforms.emplace_back(uni);
                }

                {
                    Uniform displacementUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    displacementUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(displacementUAV);
                }

                {
                    Uniform slopeUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    slopeUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(slopeUAV);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Assemble ocean pass", PipelineStateType::Compute, settings, uniforms);

                m_assembleRC.fourierTextureView = fourierData->fourierTarget->GetSrv()->BindlessView();

                list.SetPipelineState(pipeline);
                list.BeginRender("Assemble ocean pass");

                list.SetRootConstant<AssembleOceanRC>(0, m_assembleRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.displacementTexture);
                list.SetUnorderedAccessView(3, passData.slopeTexture);

                list.GetList()->Dispatch(OCEAN_SIZE / 8, OCEAN_SIZE / 8, 1);

                list.EndRender();

                passData.displacementTexture->Transition(list, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                passData.slopeTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            });
    }

    void OceanPass::AddDrawOceanPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<OceanPassData>();
        auto* pbrData = rg.GetPassData<OceanPassData, PbrPassData>();
        auto* fftOceanData = rg.GetPassData<OceanPassData, AssembleOceanPassData>();

        passData->finalTexture = pbrData->finalTexture;
        passData->depthTexture = pbrData->depthTexture;

        rg.AddPass<OceanPassData>(
            "Ocean render pass",
            PassType::Graphics,
            [&renderer, fftOceanData, this](const OceanPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/OceanRenderVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/OceanRenderFrag.slang");
                settings.DepthStencilState.DepthEnable = true;

                settings.renderTargetsFormat.push_back(passData.finalTexture->GetDesc().format);

                settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
                settings.ShaderState.InputLayout.emplace_back(
                    InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

                std::vector<Uniform> uniforms;

                Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(OceanRenderRC)};
                uniforms.emplace_back(rootConstant);

                Uniform cameraUni{1, 0, RootParams::RootResourceType::ConstantBufferView};
                uniforms.emplace_back(cameraUni);

                // Bindless resources
                {
                    Uniform uni{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE srvRange{};
                    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                  UINT_MAX,
                                  0,
                                  0,
                                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
                    CD3DX12_DESCRIPTOR_RANGE srvRange1{};
                    srvRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                   UINT_MAX,
                                   0,
                                   1,
                                   D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for cube maps
                    uni.ranges.emplace_back(srvRange);
                    uni.ranges.emplace_back(srvRange1);

                    uniforms.emplace_back(uni);
                }

                {
                    Uniform uni{0, 0, RootParams::RootResourceType::StaticSampler};
                    uni.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    uni.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    uni.samplerState.addressModeW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                    uniforms.emplace_back(uni);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Ocean render pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                auto ecs = engine.GetECS();

                Camera* camera = GetActiveCamera();

                OceanCameraData cameraData;

                if (camera)
                {
                    auto& transform = ecs->GetComponent<Transform>(m_chunkEntity);
                    cameraData.modelMatix = transform.GetWorldMatrix();
                    cameraData.projViewMatrix = camera->GetProjection() * camera->GetView();
                    cameraData.invModel = glm::transpose(glm::inverse(glm::mat3(transform.GetWorldMatrix())));
                    glm::vec3 camPos = camera->GetPosition();
                    m_oceanRC.cameraPosition = glm::vec4(camPos, 1.0f);
                }

                m_cameraBuffer->Allocate(&cameraData);

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.finalTexture},
                                 {ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store,
                                 "Ocean render pass");

                m_oceanRC.displacementMapView = fftOceanData->displacementTexture->GetSrv()->BindlessView();
                m_oceanRC.slopeMapView = fftOceanData->slopeTexture->GetSrv()->BindlessView();
                if (renderer.irradianceMap) { m_oceanRC.irradianceView = renderer.irradianceMap->GetSrv()->BindlessView(); }
                if (renderer.specularMap) { m_oceanRC.specularView = renderer.specularMap->GetSrv()->BindlessView(); }

                list.SetRootConstant<OceanRenderRC>(0, m_oceanRC);
                list.SetConstantBufferView(1, m_cameraBuffer.get());
                list.SetBindlessHeap(2);

                list.GetList()->IASetVertexBuffers(0, 1, &m_oceanVertices->GetVBView()->View());
                list.GetList()->IASetIndexBuffer(&m_oceanIndices->GetIBView()->View());
                list.GetList()->DrawIndexedInstanced(m_drawCount, 1, 0, 0, 0);

                list.EndRender();
            });
    }

    void OceanPass::GenerateOceanPlane(uint32_t resolution)
    {
        float size = 250.0f;
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
        m_oceanVertices = std::make_unique<Buffer>(vDesc);
        m_oceanVertices->CreateVertexBuffer(vertices);

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
        m_oceanIndices = std::make_unique<Buffer>(vDesc);
        m_oceanIndices->CreateIndexBuffer(indices);

        m_drawCount = indices.size();
    }

    std::pair<float, float> OceanPass::BoxMuller(float u1, float u2)
    {
        float mag = sqrtf(-2.0f * logf(u1 + 1e-6f)); // safe gaurd againts log(0)
        float z0 = mag * cosf(2.0f * 3.14159265f * u2);
        float z1 = mag * sinf(2.0f * 3.14159265f * u2);
        return {z0, z1};
    }
} // namespace Wild
