#include "Renderer/Resources/D3D12Resource.hpp"

#include "Renderer/CommandList.hpp"

namespace Wild {
	D3D12Resource::D3D12Resource(D3D12_RESOURCE_STATES initialState) : m_currentState(initialState) {}

	D3D12Resource::~D3D12Resource()
	{
		// The GPU may still use this resource in a frame that is in flight. Without a context (static destruction
		// after shutdown) the members simply release here.
		if (auto gfxContext = engine.GetGfxContext()) gfxContext->DeferRelease(std::move(m_resource), std::move(m_allocation));
	}

	void D3D12Resource::Transition(CommandList& list, D3D12_RESOURCE_STATES newState)
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