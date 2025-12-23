#pragma once

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/Resources/Buffer.hpp"

#include <unordered_map>

#include <glm/glm.hpp>

namespace Wild {
	class Texture;
	class PipelineState;

	class CommandList : private NonCopyable
	{
	public:
		CommandList(D3D12_COMMAND_LIST_TYPE listType);
		~CommandList();

		ComPtr<ID3D12GraphicsCommandList> GetList() { return m_commandList; }
		ComPtr<ID3D12CommandAllocator> GetAllocator() { return m_allocator; }

		bool IsReady() { return m_commandListClosed; }

		void ResetList();
		void Close();

		void SetPipelineState(const std::shared_ptr<PipelineState> pipeline);

		void BeginRender(const std::vector<Texture*>& renderTargets,
			const std::vector<ClearOperation>& clearRt,
			Texture* depthStencil,
			DSClearOperation clearDs,
			const std::string& passName = {});

		void EndRender();

		// TODO Implement for platform abstraction
		void SetBuffer() {};
		void SetVertexBuffer() {};
		void SetIndexBuffer() {};
		void Draw(uint32_t size) {};
	private:
		void SetRenderTargets(const std::vector<Texture*>& renderTargets, Texture* depthStencil);

		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12CommandAllocator> m_allocator;

		D3D12_COMMAND_LIST_TYPE m_type;

		// Pipeline state is set per pass
		std::shared_ptr<PipelineState> m_pipelineState{};

		bool m_commandListClosed = false;
		bool m_frameInFlight = false;
		bool m_pipelineIsSet = false;
		//std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_psoCache{};
	};
}