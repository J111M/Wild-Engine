#include "Renderer/Fence.hpp"

namespace Wild {
	Fence::Fence() : fence_value(0)
	{
		ThrowIfFailed(Wild::engine.get_device()->get_device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

		// Create fence event
		fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fence_event && "Failed to create fence event.");
	}

	Fence::~Fence()
	{
		CloseHandle(fence_event);
	}

	void Fence::signal(const CommandQueue& command_queue)
	{
		fence_value++;
		ThrowIfFailed(command_queue.get_queue()->Signal(fence.Get(), fence_value));
	}

	void Fence::signal_and_wait(const CommandQueue& command_queue)
	{
		signal(command_queue);
		wait_for_fence_value();
	}

	void Fence::wait_for_fence_value(std::chrono::milliseconds duration)
	{
		// Check if fence is completed
		if (fence->GetCompletedValue() < fence_value)
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fence_value, fence_event));
			::WaitForSingleObject(fence_event, static_cast<DWORD>(duration.count()));
		}
	}
}