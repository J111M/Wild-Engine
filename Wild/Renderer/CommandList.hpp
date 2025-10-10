#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	class CommandList
	{
	public:
		CommandList(ComPtr<ID3D12Device4> device, D3D12_COMMAND_LIST_TYPE list_type);
		~CommandList();

	private:
		void create_root_signature();

		ComPtr<ID3D12GraphicsCommandList> command_list;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12CommandAllocator> allocator;

		D3D12_COMMAND_LIST_TYPE type;

		bool root_signature_and_pso = false;
	};
}