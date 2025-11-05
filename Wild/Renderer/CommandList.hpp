#pragma once

#include "Tools/Common3d12.hpp"
#include "Renderer/ShaderPipeline.hpp"

namespace Wild {
	class CommandList : private NonCopyable
	{
	public:
		CommandList(D3D12_COMMAND_LIST_TYPE list_type);
		~CommandList() {};

		ComPtr<ID3D12GraphicsCommandList> get_list() { return command_list; }
		ComPtr<ID3D12CommandAllocator> get_allocator() { return allocator; }

		bool is_ready() { return command_list_closed; }

		void reset();
		void close();

		void BeginRender();
		void EndRender();
		//void SetPipelineSettings(const PipelineSettings& settings);

	private:
		void CreateRootSignature();

		ComPtr<ID3D12PipelineState> m_pso;

		ComPtr<ID3D12GraphicsCommandList> command_list;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12CommandAllocator> allocator;

		D3D12_COMMAND_LIST_TYPE type;

		bool command_list_closed = false;
		bool root_signature_and_pso = false;
		bool m_frameInFlight = false;

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;
	};
}