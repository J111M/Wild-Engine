#pragma once

#include "Tools/Common3d12.hpp"

#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/Resources/Buffer.hpp"

#include <unordered_map>

#include <glm/glm.hpp>

namespace Wild {
	class CommandList : private NonCopyable
	{
	public:
		CommandList(D3D12_COMMAND_LIST_TYPE listType);
		~CommandList() {};

		ComPtr<ID3D12GraphicsCommandList> GetList() { return m_commandList; }
		ComPtr<ID3D12CommandAllocator> GetAllocator() { return m_allocator; }

		bool IsReady() { return command_list_closed; }

		void Reset();
		void Close();

		void BeginRender();
		void EndRender();

		//void SetPipelineSettings(const PipelineStateSettings& settings);

	private:
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12CommandAllocator> m_allocator;

		D3D12_COMMAND_LIST_TYPE m_type;

		bool command_list_closed = false;
		bool root_signature_and_pso = false;
		bool m_frameInFlight = false;
		//std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_psoCache{};
	};
}