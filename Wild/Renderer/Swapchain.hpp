#pragma once

#include <d3d12.h>
#include "Renderer/Fence.hpp"

#include <dxgi1_6.h>

#include <chrono>

namespace Wild {
constexpr int BACK_BUFFER_COUNT = 2;

class Device;

	class Swapchain
	{
	public:
		Swapchain(std::shared_ptr<Window> p_window);
		~Swapchain() {};

		HRESULT present(uint32_t sync_interval, uint32_t present_flag);

		UINT get_back_buffer_index() { return swapchain->GetCurrentBackBufferIndex(); }
		ComPtr<ID3D12Resource> get_render_target(int index) { return render_targets[index]; }

		ComPtr<ID3D12DescriptorHeap> get_rtv_heap() { return rtv_heap; }
		UINT get_rtv_size() { return rtv_descriptor_size; }

		void resize();
	private:
		void recreate_swapchain();

		UINT frame_index{};

		// Handles
		UINT current_buffer{};
		ComPtr<ID3D12DescriptorHeap> rtv_heap;
		ComPtr<ID3D12Resource> render_targets[BACK_BUFFER_COUNT];
		UINT rtv_descriptor_size;

		// Swapchain
		ComPtr<IDXGISwapChain3> swapchain;
		D3D12_VIEWPORT viewport;
		D3D12_RECT surface_size;

		std::shared_ptr<Window> window;
	};
}