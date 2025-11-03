#pragma once

#include "Renderer/CommandQueue.hpp"

#include <chrono>

namespace Wild {
	class Fence : private NonCopyable
	{
	public:
		Fence();
		~Fence();

		void signal(const CommandQueue& command_queue);
		void signal_and_wait(const CommandQueue& command_queue);

		void wait_for_fence_value(std::chrono::milliseconds duration = std::chrono::milliseconds::max());
	private:
		ComPtr<ID3D12Fence> fence;
		uint64_t fence_value;
		HANDLE fence_event;
	};
}