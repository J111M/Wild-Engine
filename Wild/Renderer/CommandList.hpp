#pragma once

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

#include "Renderer/ShaderPipeline.hpp"

#include <optional>
#include <unordered_map>

#include <glm/glm.hpp>

namespace Wild
{
    class Texture;
    class PipelineState;
    class Buffer;

    class CommandList : private NonCopyable
    {
      public:
        CommandList(D3D12_COMMAND_LIST_TYPE listType);
        ~CommandList();

        ComPtr<ID3D12GraphicsCommandList> GetList() { return m_commandList; }
        ComPtr<ID3D12GraphicsCommandList4> GetList4() { return m_commandList4; }
        ComPtr<ID3D12CommandAllocator> GetAllocator() { return m_allocator; }

        bool IsReady() const { return m_commandListClosed; }

        void ResetList();
        void Close();

        void SetPipelineState(const std::shared_ptr<PipelineState> pipeline);

        void BeginRender(const std::vector<Texture*>& renderTargets, const std::vector<ClearOperation>& clearRt,
                         Texture* depthStencil, DSClearOperation clearDs, const std::string& passName = {},
                         std::optional<uint32_t> rtArrayIndex = std::nullopt);
        void BeginRender(const std::string& passName = {});

        void EndRender();

        void ClearDepthStencil(Texture& depthStencil, const DSClearOperation clear, const float depth = 1.0,
                               const uint8_t stencil = 0);
        void ClearRenderTarget(Texture& renderTarget, const glm::vec4& color = {0.0f, 0.0f, 0.0f, 1.0f});

        // TODO Implement for platform abstraction

        template <typename rc> void SetRootConstant(uint32_t rootIndex, rc& rootConstant);
        void SetBindlessHeap(uint32_t rootIndex);
        void SetConstantBufferView(uint32_t rootIndex, Buffer* buffer);

        void SetUnorderedAccessView(uint32_t rootIndex, Buffer* buffer);
        void SetUnorderedAccessView(uint32_t rootIndex, Texture* texture, std::optional<uint32_t> index = std::nullopt);

        void SetShaderResourceView(uint32_t rootIndex, Buffer* buffer);

        void SetVertexBuffer() {};
        void SetIndexBuffer() {};
        void Draw(uint32_t size) {};

        void Dispatch(uint32_t x = 1u, uint32_t y = 1u, uint32_t z = 1u);

      private:
        void SetRenderTargets(const std::vector<Texture*>& renderTargets, Texture* depthStencil,
                              std::optional<uint32_t> rtArrayIndex = std::nullopt);

        const bool CanPassExecute(const std::string& passName);

        uint32_t GetPassColor(std::string_view name);

        ComPtr<ID3D12GraphicsCommandList> m_commandList;

        // For raytracing features
        ComPtr<ID3D12GraphicsCommandList4> m_commandList4;

        ComPtr<ID3D12CommandAllocator> m_allocator;

        D3D12_COMMAND_LIST_TYPE m_type;

        // Pipeline state is set per pass
        std::shared_ptr<PipelineState> m_pipelineState{};

        bool m_commandListClosed = false;
        bool m_frameInFlight = false;
        bool m_pipelineIsSet = false;
        // std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_psoCache{};
    };

    template <typename rc> inline void CommandList::SetRootConstant(uint32_t rootIndex, rc& rootConstant)
    {
        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
            m_commandList->SetGraphicsRoot32BitConstants(static_cast<UINT>(rootIndex), sizeof(rc) / 4, &rootConstant, 0);
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRoot32BitConstants(static_cast<UINT>(rootIndex), sizeof(rc) / 4, &rootConstant, 0);
            break;
        case PipelineStateType::MeshPipeline:
            break;
        default:
            break;
        }
    }
} // namespace Wild
