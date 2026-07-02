#include "Renderer/Resources/Resource3d12.hpp"

#include "Renderer/CommandList.hpp"

namespace Wild {
	Resource::Resource(D3D12_RESOURCE_STATES initialState) : m_currentState(initialState) {}

	void Resource::Transition(CommandList& list, D3D12_RESOURCE_STATES newState)
	{
		if (m_currentState == newState)
			return;

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_resource.Get(),
			m_currentState, newState);

		m_currentState = newState;

		list.GetList()->ResourceBarrier(1, &barrier);
	}
}