#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	class Resource
	{
	public:

		Resource(D3D12_RESOURCE_STATES initialState);
		~Resource() {};

		ComPtr<ID3D12Resource>& Handle() { return m_resource; }
		void SetResource(ComPtr<ID3D12Resource> resource) { m_resource = resource; }

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