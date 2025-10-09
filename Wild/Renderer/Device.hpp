#pragma once

#include "Renderer/Swapchain.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace Wild {
	class Device
	{
	public:
		Device(Window& window);
		~Device() {};

		ComPtr<ID3D12Device> get_device() { return device; }
		ComPtr<IDXGIFactory4> get_factory() { return factory; }
	private:
		void setup_factory();
		void create_adapter();
		void create_device();

		ComPtr<IDXGIFactory4> factory;

		// factory flags
		UINT dxgi_factory_flags = 0;

		ComPtr<ID3D12Debug1> debug_controller;

		ComPtr<IDXGIAdapter1> adapter;

		ComPtr<ID3D12Device> device;
		ComPtr<ID3D12DebugDevice> debug_device;

		std::unique_ptr<Swapchain> swapchain;
	};
}