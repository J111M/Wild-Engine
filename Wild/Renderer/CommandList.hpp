#pragma once

#include <d3d12.h>

namespace Wild {
	class CommandList
	{
	public:
		CommandList();
		~CommandList();

	private:
		void create_root_signature();

		ComPtr<ID3D12GraphicsCommandList> command_list;
		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12CommandAllocator> allocator;

	};
}