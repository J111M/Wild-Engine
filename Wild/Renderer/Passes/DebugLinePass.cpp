#include "Renderer/Passes/DebugLinePass.hpp"
#include "Renderer/Passes/PostProcessPass.hpp"

#include "Core/Camera.hpp"

namespace Wild
{
    void DebugLinePass::AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color)
    {
        if (m_lines.size() + 2 > MAX_VERTS) return;
        m_lines.push_back({a, color});
        m_lines.push_back({b, color});
    }

    void DebugLinePass::AddAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color)
    {
        glm::vec3 corners[8] = {
            {min.x, min.y, min.z},
            {max.x, min.y, min.z},
            {max.x, max.y, min.z},
            {min.x, max.y, min.z},
            {min.x, min.y, max.z},
            {max.x, min.y, max.z},
            {max.x, max.y, max.z},
            {min.x, max.y, max.z},
        };
        int edges[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
        for (int i = 0; i < 24; i += 2)
            AddLine(corners[edges[i]], corners[edges[i + 1]], color);
    }

    DebugLinePass::DebugLinePass() {}

    void DebugLinePass::Add(Renderer& renderer, RenderGraph& rg)
    {
        auto* passData = rg.AllocatePassData<DebugLinePassData>();
        auto* finalPassData = rg.GetPassData<DebugLinePassData, FinalPostProcessPassData>();

        passData->debugTexture = finalPassData->finalTexture;

        rg.AddPass<DebugLinePassData>(
            "Debug line pass", PassType::Graphics, [&renderer, this](DebugLinePassData& passData, CommandList& list) {
                // Write this frame's lines straight into an upload-heap buffer: no per-frame allocation and no GPU wait
                const uint64_t lineDataSize = static_cast<uint64_t>(m_lines.size()) * sizeof(DebugVertex);

                D3D12_VERTEX_BUFFER_VIEW lineVertexView{};
                if (lineDataSize > 0)
                {
                    auto& vertexBuffer = m_lineVertexBuffers.Get();
                    EnsureUploadBuffer(vertexBuffer, lineDataSize, "Debug line vertices");
                    vertexBuffer->Allocate(m_lines.data(), lineDataSize);

                    lineVertexView.BufferLocation = vertexBuffer->GetBuffer()->GetGPUVirtualAddress();
                    lineVertexView.SizeInBytes = static_cast<UINT>(lineDataSize);
                    lineVertexView.StrideInBytes = sizeof(DebugVertex);
                }

                PipelineStateSettings settings{};
                settings.shaderState.vertexShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/lineDebugVert.slang");
                settings.shaderState.fragShader =
                    engine.GetShaderTracker()->GetOrCreateShader("Shaders/DebugTools/lineDebugFrag.slang");
                settings.depthStencilState.depthEnable = false;
                settings.rasterizerState.topologyMode = PrimitiveTopology::LineList;

                // Setting up the input layout
                settings.shaderState.inputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
                settings.shaderState.inputLayout.emplace_back(
                    InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));

                settings.renderTargetsFormat.push_back(passData.debugTexture->GetDesc().format);

                std::vector<Uniform> uniforms;
                Uniform sceneCameraUni{0, 0, RootParams::RootResourceType::ConstantBufferView};
                sceneCameraUni.visibility = D3D12_SHADER_VISIBILITY_VERTEX;
                uniforms.emplace_back(sceneCameraUni);

                auto& pipeline = renderer.GetOrCreatePipeline("Debug line pass", PipelineStateType::Graphics, settings, uniforms);

                // Rendering
                list.SetPipelineState(pipeline);
                list.BeginRender(
                    {passData.debugTexture}, {ClearOperation::Store}, {nullptr}, DSClearOperation::Store);

                list.GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

                list.SetConstantBufferView(0, GetSceneDataAddress());
                if (lineDataSize > 0)
                {
                    list.GetList()->IASetVertexBuffers(0, 1, &lineVertexView);
                    list.GetList()->DrawInstanced(static_cast<UINT>(m_lines.size()), 1, 0, 0);
                }

                list.EndRender();

                m_lines.clear();
                passData.debugTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                renderer.compositeTexture = passData.debugTexture;
            });
    }

    void DebugLinePass::Update(const float dt) {}
} // namespace Wild
