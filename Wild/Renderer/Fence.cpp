#include "Renderer/Fence.hpp"

namespace Wild {
	Fence::Fence() : m_fenceValue(0)
	{
		ThrowIfFailed(Wild::engine.GetDevice()->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

		// Create fence event
		m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(m_fenceEvent && "Failed to create fence event.");
	}

	Fence::~Fence()
	{
		CloseHandle(m_fenceEvent);
	}

	void Fence::Signal(const CommandQueue& m_commandQueue)
	{
		m_fenceValue++;
		ThrowIfFailed(m_commandQueue.get_queue()->Signal(m_fence.Get(), m_fenceValue));
	}

	void Fence::SignalAndWait(const CommandQueue& m_commandQueue)
	{
		Signal(m_commandQueue);
		WaitForFenceValue();
	}

	void Fence::WaitForFenceValue(std::chrono::milliseconds duration)
	{
		// Check if fence is completed
		if (m_fence->GetCompletedValue() < m_fenceValue)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
			::WaitForSingleObject(m_fenceEvent, static_cast<DWORD>(duration.count()));
		}
	}
}