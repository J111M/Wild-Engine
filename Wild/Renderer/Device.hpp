#pragma once

#include <d3d12.h>

#include <dxgi1_6.h>

namespace Wild {
	class Device
	{
	public:
		Device(Window& window);
		~Device() {};

		ComPtr<ID3D12Device> get_device() { return m_device; }
	private:
		void setup_factory();
		void create_adapter();
		void create_device();

		ComPtr<IDXGIFactory4> m_factory;

		// factory flags
		UINT m_dxgi_factory_flags = 0;


		ComPtr<ID3D12Debug1> m_debug_controller;

		ComPtr<IDXGIAdapter1> m_adapter;

		ComPtr<ID3D12Device> m_device;
		ComPtr<ID3D12DebugDevice> m_debugDevice;
	};
}