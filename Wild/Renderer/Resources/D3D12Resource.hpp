#pragma once

#include "Tools/D3D12Common.hpp"

namespace Wild {
	class D3D12Resource
	{
	public:

		D3D12Resource(D3D12_RESOURCE_STATES initialState);
		~D3D12Resource() {};

		ComPtr<ID3D12Resource>& Handle() { return m_resource; }

		// Prevent copy by moving the data
		void SetResource(ComPtr<ID3D12Resource> resource) { m_resource = std::move(resource); }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_resource->GetGPUVirtualAddress(); }

		D3D12_RESOURCE_DESC GetDesc() const { return m_resource->GetDesc(); }
		D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }

		void Transition(CommandList& list, D3D12_RESOURCE_STATES newState);
		//void Transition(CommandList& list, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState);
	private:
		D3D12_RESOURCE_STATES m_currentState;
		ComPtr<ID3D12Resource> m_resource;
	};
}