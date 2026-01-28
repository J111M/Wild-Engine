#include "Renderer/Passes/ProceduralTerrainPass.hpp"

namespace Wild
{
    ProceduralTerrainPass::ProceduralTerrainPass() {}

    void ProceduralTerrainPass::Update(const float dt) {}

    void ProceduralTerrainPass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto *passData = rg.AllocatePassData<ProceduralTerrainPassData>();

        rg.AddPass<ProceduralTerrainPassData>(
            "Procedural terrain pass",
            PassType::Compute,
            [&renderer, this](ProceduralTerrainPassData& passData, CommandList& list) {
                if (m_shouldGenerateChunks)
                {
                    TextureDesc desc{};
                    desc.width = 512;
                    desc.Height = 512;
                    desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::readWrite | TextureDesc::shaderResource);
                    desc.format = DXGI_FORMAT_R32_FLOAT;

                    const uint32_t chunkWidth = 1u;
                    const uint32_t chunkHeight = 1u;

                    // Chunk size
                    for (uint32_t x = 0; x < chunkWidth; x++)
                    {
                        for (uint32_t y = 0; y < chunkHeight; y++)
                        {
                            desc.name = "Chunk number X: " + std::to_string(x) + " | Y: " + std::to_string(y);
                            heightMaps.emplace_back(std::make_shared<Texture>(desc));
                        }
                    }

                    PipelineStateSettings computeSettings{};
                    computeSettings.ShaderState.ComputeShader =
                        engine.GetShaderTracker()->GetOrCreateShader("Shaders/ProceduralTerrainCompute.slang");

                    std::vector<Uniform> uniforms;

                    Uniform rootConstant{0, 0, RootParams::RootResourceType::Constants, sizeof(TerrainRootConstant)};
                    uniforms.emplace_back(rootConstant);

                    // UAV uniform for specular cube map
                    Uniform heightMap{0, 0, RootParams::RootResourceType::DescriptorTable};
                    CD3DX12_DESCRIPTOR_RANGE uavRange{};
                    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                    heightMap.ranges.emplace_back(uavRange);
                    uniforms.emplace_back(heightMap);

                    auto& pipeline = renderer.GetOrCreatePipeline(
                        "Procedural terrain pass", PipelineStateType::Compute, computeSettings, uniforms);

                    for (size_t i = 0; i < heightMaps.size(); i++)
                    {
                        list.SetPipelineState(pipeline);
                        list.BeginRender();
                        m_rc.textureSize = glm::vec2(desc.width, desc.Height);

                        size_t x = i % chunkWidth;
                        size_t y = i / chunkWidth;
                        m_rc.chunkPosition = glm::vec2(x * 32, y * 32);

                        list.SetRootConstant(0, m_rc);
                        list.SetUnorderedAccessView(1, heightMaps[i].get());
                        list.GetList()->Dispatch((desc.width + 31) / 32, (desc.Height + 31) / 32, 1);

                        list.EndRender();
                    }

                    m_shouldGenerateChunks = false;
                }
            });
    }
} // namespace Wild
