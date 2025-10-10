#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	class CommandQueue
	{
	public:
		CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE list_type, const std::string& resource_name);
		~CommandQueue() {};

		ComPtr<ID3D12CommandQueue> get_queue() const { return command_queue; }

		D3D12_COMMAND_LIST_TYPE get_type() const { return type; }
	private:
		ComPtr<ID3D12CommandQueue> command_queue;
		D3D12_COMMAND_LIST_TYPE type{};
	};
}