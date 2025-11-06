#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	class Resource3d12
	{
	public:
		Resource3d12() {};
		~Resource3d12() {};

		ComPtr<ID3D12Resource> GetResource() { return m_resource; }

		void Transition(ComPtr<ID3D12GraphicsCommandList> list, D3D12_RESOURCE_STATES newState);
	private:
		D3D12_RESOURCE_STATES m_currentState;
		ComPtr<ID3D12Resource> m_resource;
	};


}