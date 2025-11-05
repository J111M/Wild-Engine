#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/CommandQueue.hpp"
#include "Renderer/Resources/Texture.hpp"

#include "Tools/DescriptorAllocator.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace Wild {
	constexpr int BACK_BUFFER_COUNT = 2;

	class Device
	{
	public:
		Device(std::shared_ptr<Window> p_window);
		~Device() {};

		void initialize();

		ComPtr<ID3D12Device> get_device() { return device; }
		ComPtr<IDXGIFactory4> get_factory() { return factory; }

		std::shared_ptr<CommandQueue> get_command_queue() { return command_queue; }

		UINT get_back_buffer_index() { return m_swapchain->GetCurrentBackBufferIndex(); }
		

		void begin_frame();
		void end_frame();

		void resize_window();

		int GetWidth() { return client_width; }
		int GetHeight() { return client_height; }

		void flush();

		std::shared_ptr<DescriptorAllocatorRtv> GetRtvAllocator() { return m_descriptorAllocatorsRtv; }
		std::shared_ptr<DescriptorAllocatorDsv> GetDsvAllocator() { return m_descriptorAllocatorsDsv; }
	private:
		void setup_factory();
		void create_adapter();
		void create_device();
		void CreateSwapchain();

		void CreateTextureFromSwapchain(UINT index);

		ComPtr<IDXGIFactory4> factory;

		// factory flags
		UINT dxgi_factory_flags = 0;

		ComPtr<ID3D12Debug1> debug_controller;

		ComPtr<IDXGIAdapter1> adapter;

		ComPtr<ID3D12Device> device;
		ComPtr<ID3D12DebugDevice> debug_device;

		// Swapchain
		ComPtr<IDXGISwapChain3> m_swapchain;
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_surfaceSize;

		std::shared_ptr<CommandList> command_list[BACK_BUFFER_COUNT];
		std::shared_ptr<Texture> m_renderTargets[BACK_BUFFER_COUNT];

		// TODO make command queue for each type
		std::shared_ptr<CommandQueue> command_queue;

		std::shared_ptr<Window> window;

		std::shared_ptr<DescriptorAllocatorRtv> m_descriptorAllocatorsRtv;
		std::shared_ptr<DescriptorAllocatorDsv> m_descriptorAllocatorsDsv;

		UINT back_buffer_index{};
		UINT current_frame{};

		bool is_vsync_enabled = true;
		bool is_hdr_enabled = false;

		int client_width{}, client_height{};
	};
}