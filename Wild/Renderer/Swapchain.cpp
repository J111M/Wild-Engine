#include "Renderer/Swapchain.hpp"

#include "Renderer/Device.hpp"

namespace Wild {
	Swapchain::Swapchain(std::shared_ptr<Window> p_window, Device& device)
	{
		window = p_window;

		// Setup surface area and viewport size
		surface_size.left = 0;
		surface_size.top = 0;
		surface_size.right = static_cast<LONG>(window->get_width());
		surface_size.bottom = static_cast<LONG>(window->get_height());

		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(window->get_width());
		viewport.Height = static_cast<float>(window->get_height());
		viewport.MinDepth = .1f;
		viewport.MaxDepth = 1000.f;

		create_fence(device);
		recreate_swapchain(device);
	}

	void Swapchain::create_fence(Device& device)
	{
		ThrowIfFailed(device.get_device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	}

	void Swapchain::recreate_swapchain(Device& device)
	{
		if (swapchain != nullptr)
			swapchain->ResizeBuffers(BACK_BUFFER_COUNT, window->get_width(), window->get_height(),
				DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		else {
			// If there is no swapchain create one
			DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
			swapchain_desc.BufferCount = BACK_BUFFER_COUNT;
			swapchain_desc.Width = window->get_width();
			swapchain_desc.Height = window->get_height();
			swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapchain_desc.SampleDesc.Count = 1;

			ComPtr<IDXGISwapChain1> new_swapchain;

			ThrowIfFailed(device.get_factory()->CreateSwapChainForHwnd(device.get_command_queue()->get_queue().Get(), window->get_handle(), &swapchain_desc, nullptr, nullptr, &new_swapchain), "Swapchain creation failed");

			ThrowIfFailed(new_swapchain.As(&swapchain));
		}

		frame_index = swapchain->GetCurrentBackBufferIndex();

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		ThrowIfFailed(device.get_device()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&render_target_view_heap)));

		rtv_descriptor_size = device.get_device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

		for (UINT n = 0; n < BACK_BUFFER_COUNT; n++)
		{
			ThrowIfFailed(swapchain->GetBuffer(n, IID_PPV_ARGS(&render_targets[n])));
			device.get_device()->CreateRenderTargetView(render_targets[n].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += (1 * rtv_descriptor_size);
		}
	}
}