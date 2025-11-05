#pragma once

#include "Tools/Common3d12.hpp"

#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/Resources/Buffer.hpp"

#include <unordered_map>

#include <glm/glm.hpp>

namespace Wild {
	struct Vertex {
		glm::vec3 position{};
	};

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

		//void SetPipelineSettings(const PipelineStateSettings& settings);

	private:
		ComPtr<ID3D12GraphicsCommandList> command_list;
		ComPtr<ID3D12CommandAllocator> allocator;

		D3D12_COMMAND_LIST_TYPE type;

		bool command_list_closed = false;
		bool root_signature_and_pso = false;
		bool m_frameInFlight = false;
		//std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_psoCache{};
	};
}