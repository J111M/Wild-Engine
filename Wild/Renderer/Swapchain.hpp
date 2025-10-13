#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include <chrono>

namespace Wild {
constexpr int BACK_BUFFER_COUNT = 2;

class Device;

	class Swapchain
	{
	public:
		Swapchain(std::shared_ptr<Window> p_window, Device& device);
		~Swapchain() {};

		ComPtr<ID3D12Fence> get_fence() { return fence; }

	private:
		void create_fence(Device& device);
		void recreate_swapchain(Device& device);

		void wait_for_fence(std::chrono::milliseconds duration = std::chrono::milliseconds::max());

		UINT frame_index{};
		HANDLE fence_event{};
		ComPtr<ID3D12Fence> fence;
		UINT64 fence_value{};

		// Handles
		UINT current_buffer;
		ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
		ComPtr<ID3D12Resource> render_targets[BACK_BUFFER_COUNT];
		UINT rtv_descriptor_size;

		// Swapchain
		ComPtr<IDXGISwapChain3> swapchain;
		D3D12_VIEWPORT viewport;
		D3D12_RECT surface_size;

		// Command queue
		ComPtr<ID3D12CommandQueue> command_queue;

		std::shared_ptr<Window> window;
	};
}