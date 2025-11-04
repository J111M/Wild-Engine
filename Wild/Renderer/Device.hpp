#pragma once

#include "Renderer/Swapchain.hpp"
#include "Renderer/CommandList.hpp"
#include "Renderer/CommandQueue.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace Wild {
	class Device
	{
	public:
		Device(std::shared_ptr<Window> p_window);
		~Device() {};

		void initialize();

		ComPtr<ID3D12Device> get_device() { return device; }
		ComPtr<IDXGIFactory4> get_factory() { return factory; }

		std::shared_ptr<CommandQueue> get_command_queue() { return command_queue; }

		UINT get_back_buffer_index() { return swapchain->get_back_buffer_index(); }

		void begin_frame();
		void end_frame();

		void resize_window();

		void flush();
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

		std::shared_ptr<CommandList> command_list[BACK_BUFFER_COUNT];

		// TODO make command queue for each type
		std::shared_ptr<CommandQueue> command_queue;
		std::unique_ptr<Swapchain> swapchain;

		std::shared_ptr<Window> window;

		UINT back_buffer_index{};
		UINT current_frame{};

		bool is_vsync_enabled = true;
		bool is_hdr_enabled = false;

		int client_width{}, client_height{};
	};
}