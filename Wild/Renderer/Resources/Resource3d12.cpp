#include "Renderer/Resources/Resource3d12.hpp"

namespace Wild {
	void Resource3d12::Transition(CommandList& list, D3D12_RESOURCE_STATES newState)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_resource.Get(),
			m_currentState, newState);

		m_currentState = newState;

		list.GetList()->ResourceBarrier(1, &barrier);
	}
}