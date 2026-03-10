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

        BufferDesc desc{};
        desc.bufferSize = sizeof(ComplexValue);
        desc.numOfElements = val.size();
        m_gaussianDistribution = std::make_unique<Buffer>(desc, uav);

        m_gaussianDistribution->UploadToGPU(val.data());

        GenerateOceanPlane(64);
        m_chunkEntity = engine.GetECS()->CreateEntity();
        auto& transform = engine.GetECS()->AddComponent<Transform>(m_chunkEntity, glm::vec3(0, 0, 0), m_chunkEntity);
        transform.SetPosition(glm::vec3(0, -9, 0));
    }

    void OceanPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        CalculateIntitialSpectrum(renderer, rg);
        ConjugateSpectrumPass(renderer, rg);
        UpdateSpectrum(renderer, rg);
        AddDrawOceanPass(renderer, rg);
    }

    void OceanPass::Update(const float dt)
    {
        engine.GetImGui()->AddPanel("Ocean Settings", [this]() {
            ImGui::SliderFloat("Scale", &m_initialSpectrumRC.scale, 0.0f, 1000.0f);
            ImGui::SliderFloat("Angle", &m_initialSpectrumRC.angle, -3.14159f, 3.14159f);
            ImGui::SliderFloat("Spread Blend", &m_initialSpectrumRC.spreadBlend, 0.0f, 1.0f);
            ImGui::SliderFloat("Swell", &m_initialSpectrumRC.swell, 0.0f, 1.0f);
            ImGui::SliderFloat("Alpha", &m_initialSpectrumRC.alpha, 0.0f, 1.0f);
            ImGui::SliderFloat("Peak Omega", &m_initialSpectrumRC.peakOmega, 0.0f, 10.0f);
            ImGui::SliderFloat("Gamma", &m_initialSpectrumRC.gamma, 0.0f, 10.0f);
            ImGui::SliderFloat("Short Waves Fade", &m_initialSpectrumRC.shortWavesFade, 0.0f, 1.0f);

            ImGui::Checkbox("Recompute Initial spectrum", &m_recomputeInitialSpectrum);
        });

        m_updateSpectrumRC.time += dt;
    }

    void OceanPass::CalculateIntitialSpectrum(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<InitialSpectrumPassData>();

        TextureDesc desc;
        desc.width = OCEAN_SIZE;
        desc.height = OCEAN_SIZE;
        desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Initial spectrum ocean pass", PipelineStateType::Compute, settings, uniforms);
                    list.SetPipelineState(pipeline);
                    list.BeginRender("Initial spectrum ocean pass");

                    list.SetRootConstant<InitialSpectrumRC>(0, m_initialSpectrumRC);
                    list.SetShaderResourceView(1, m_gaussianDistribution.get());
                    list.SetUnorderedAccessView(2, passData.initialSpectrumTexture);

                    list.GetList()->Dispatch((desc.width + 7) / 8, (desc.height + 7) / 8, 1);

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

                    list.GetList()->Dispatch((OCEAN_SIZE + 7) / 8, (OCEAN_SIZE + 7) / 8, 1);

                    list.EndRender();

                    passData.conjugatedTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    m_recomputeInitialSpectrum = false;
                }

                // engine.GetImGui()->AddPanel("Spectrum texture", [this, passData]() {
                //     engine.GetImGui()->DisplayTexture(passData.conjugatedTexture, {256, 256});
                // });
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
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "SpectrumTexture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->spectrumTexture = rg.CreateTransientTexture("SpectrumTexture", desc);
        }

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "SpectrumTexture2";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->spectrumTexture1 = rg.CreateTransientTexture("SpectrumTexture2", desc);
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

                {
                    Uniform slopeUAV{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    slopeUAV.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(slopeUAV);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Update spectrum ocean pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Update spectrum ocean pass");

                m_updateSpectrumRC.h0TextureView = conjugateSpectrumData->conjugatedTexture->GetSrv()->BindlessView();

                list.SetRootConstant<UpdateSpectrumRC>(0, m_updateSpectrumRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.spectrumTexture);
                list.SetUnorderedAccessView(3, passData.spectrumTexture1);

                list.GetList()->Dispatch((OCEAN_SIZE + 7) / 8, (OCEAN_SIZE + 7) / 8, 1);

                list.EndRender();

                passData.spectrumTexture->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                passData.spectrumTexture1->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            });
    }

    void OceanPass::FastFourierPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<IFFTPassData>();
        auto* spectrumData = rg.GetPassData<IFFTPassData, UpdateSpectrumPassData>();

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "Displacement texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->displacementMap = rg.CreateTransientTexture("Displacement texture", desc);
        }

        {
            TextureDesc desc;
            desc.width = OCEAN_SIZE;
            desc.height = OCEAN_SIZE;
            desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            desc.name = "Slope texture";
            desc.usage = TextureDesc::gpuOnly;
            desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
            desc.shouldStayInCache = true;
            passData->slopeMap = rg.CreateTransientTexture("Slope texture", desc);
        }

        rg.AddPass<IFFTPassData>(
            "Inverse FFT ocean pass", PassType::Compute, [&renderer, this](IFFTPassData& passData, CommandList& list) {
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
                    renderer.GetOrCreatePipeline("Inverse FFT ocean pass", PipelineStateType::Compute, settings, uniforms);
                list.SetPipelineState(pipeline);
                list.BeginRender("Inverse FFT ocean pass");

                list.SetRootConstant<IFFTRC>(0, m_ifftRC);
                list.SetBindlessHeap(1);
                list.SetUnorderedAccessView(2, passData.displacementMap);
                list.SetUnorderedAccessView(3, passData.slopeMap);

                list.GetList()->Dispatch((OCEAN_SIZE + 7) / 8, (OCEAN_SIZE + 7) / 8, 1);

                list.EndRender();

                engine.GetImGui()->AddPanel("Spectrum texture", [this, passData]() {
                    engine.GetImGui()->DisplayTexture(passData.displacementMap, {256, 256});
                    engine.GetImGui()->DisplayTexture(passData.slopeMap, {256, 256});
                });
            });
    }

    void OceanPass::AddDrawOceanPass(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<OceanPassData>();
        auto* pbrData = rg.GetPassData<OceanPassData, PbrPassData>();

        passData->finalTexture = pbrData->finalTexture;
        passData->depthTexture = pbrData->depthTexture;

        rg.AddPass<OceanPassData>(
            "Ocean render pass", PassType::Graphics, [&renderer, this](const OceanPassData& passData, CommandList& list) {
                PipelineStateSettings settings{};
                settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/OceanRenderVert.slang");
                settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/OceanRenderFrag.slang");
                settings.DepthStencilState.DepthEnable = true;

                settings.renderTargetsFormat.push_back(passData.finalTexture->GetDesc().format);

                settings.RasterizerState.BlendDesc.RenderTarget[0].BlendEnable = true;
                // settings.RasterizerState.BlendDesc.RenderTarget[0].logic = FALSE;
                settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlend = Blend::SrcAlpha;
                settings.RasterizerState.BlendDesc.RenderTarget[0].DestBlend = Blend::InvSrcAlpha;
                settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOperation = BlendOp::Add;
                settings.RasterizerState.BlendDesc.RenderTarget[0].SrcBlendAlpha = Blend::One;
                settings.RasterizerState.BlendDesc.RenderTarget[0].DestBlendAlpha = Blend::Zero;
                settings.RasterizerState.BlendDesc.RenderTarget[0].BlendOperationAlpha = BlendOp::Add;
                settings.RasterizerState.BlendDesc.RenderTarget[0].RenderTargetWriteMask = ColorWrite::EnableAll;

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
                    uni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;

                    uniforms.emplace_back(uni);
                }

                {
                    Uniform uni{0, 0, RootParams::RootResourceType::StaticSampler};
                    uni.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
                    uni.samplerState.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                    uni.samplerState.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                    uniforms.emplace_back(uni);
                }

                auto& pipeline =
                    renderer.GetOrCreatePipeline("Ocean render pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                auto ecs = engine.GetECS();

                Camera* camera = GetActiveCamera();

                list.SetPipelineState(pipeline);
                list.BeginRender({passData.finalTexture},
                                 {ClearOperation::Store},
                                 passData.depthTexture,
                                 DSClearOperation::Store,
                                 "Ocean render pass");

                if (camera)
                {
                    auto& transform = ecs->GetComponent<Transform>(m_chunkEntity);
                    m_oceanRc.worldMatix = camera->GetProjection() * camera->GetView() * transform.GetWorldMatrix();
                    m_oceanRc.invModel = glm::transpose(glm::inverse(glm::mat3(transform.GetWorldMatrix())));
                }

                list.SetRootConstant<OceanRenderRC>(0, m_oceanRc);
                list.SetBindlessHeap(1);

                list.GetList()->IASetVertexBuffers(0, 1, &m_oceanVertices->GetVBView()->View());
                list.GetList()->IASetIndexBuffer(&m_oceanIndices->GetIBView()->View());
                list.GetList()->DrawIndexedInstanced(m_drawCount, 1, 0, 0, 0);

                list.EndRender();
            });
    }

    void OceanPass::GenerateOceanPlane(uint32_t resolution)
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
