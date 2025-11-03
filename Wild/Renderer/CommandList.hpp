#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	class CommandList : private NonCopyable
	{
	public:
		CommandList(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE list_type);
		~CommandList() {};

		ComPtr<ID3D12GraphicsCommandList> get_list() { return command_list; }
		ComPtr<ID3D12CommandAllocator> get_allocator() { return allocator; }

		void reset();
	private:
		void create_root_signature();

		ComPtr<ID3D12GraphicsCommandList> command_list;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12CommandAllocator> allocator;

		D3D12_COMMAND_LIST_TYPE type;

		bool root_signature_and_pso = false;
	};
}