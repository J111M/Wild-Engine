#include "Renderer/Device.hpp"

#include <initguid.h>
#include <codecvt>
#include <locale>

namespace Wild {
    Device::Device(std::shared_ptr<Window> p_window)
    {
        window = p_window;

        client_width = window->get_width();
        client_height = window->get_height();
    }

    void Device::initialize() {
        setup_factory();
        create_adapter();
        create_device();

        command_queue = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct queue");
        swapchain = std::make_unique<Swapchain>(window);

        for (size_t i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            command_list[i] = std::make_shared<CommandList>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
    }

    void Device::begin_frame()
    {
        resize_window();

        current_frame = back_buffer_index;
        back_buffer_index = get_back_buffer_index();

        auto current_command_list = command_list[current_frame];
        auto back_buffer = swapchain->get_render_target(back_buffer_index);

        // Set the rt to the render target state for clearing
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            back_buffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        current_command_list->get_list()->ResourceBarrier(1, &barrier);

        FLOAT clear_color[] = { 0.4f, 0.6f, 0.9f, 1.0f };

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(swapchain->get_rtv_heap()->GetCPUDescriptorHandleForHeapStart(),
            back_buffer_index, swapchain->get_rtv_size());

        current_command_list->get_list()->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
    }

    void Device::end_frame()
    {
        auto current_command_list = command_list[current_frame];
        auto back_buffer = swapchain->get_render_target(back_buffer_index);

        // Present frame
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            back_buffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        current_command_list->get_list()->ResourceBarrier(1, &barrier);

        ThrowIfFailed(current_command_list->get_list()->Close());

        ID3D12CommandList* const commandLists[] = {
           current_command_list->get_list().Get()
        };

        command_queue->get_queue()->ExecuteCommandLists(_countof(commandLists), commandLists);
        command_queue->wait_for_fence();

        uint32_t flags = DXGI_PRESENT_ALLOW_TEARING;
        UINT sync_interval = 0;

        if (is_vsync_enabled) {
            flags = 0;
            sync_interval = 1;
        }

        ThrowIfFailed(swapchain->present(sync_interval, flags));

        // Reset allocator and list
        current_command_list->reset();
    }

    void Device::resize_window()
    {
        int window_width = window->get_width();
        int window_height = window->get_height();

        if (client_width != window_width  || client_height != window_height) {

            // We don't want the back buffer to be smaller than 1
            client_width = std::max(1, window_width);
            client_height = std::max(1, window_height);

            // Wait till all commands are flushed
            command_queue->wait_for_fence();

            swapchain->resize();

            WD_INFO("Window is resized succsesfully!");

            client_width = window->get_width();
            client_height = window->get_height();
        }
    }

    void Device::flush() {
        command_queue->wait_for_fence();
    }

    void Device::setup_factory()
    {
#if defined(_DEBUG)
        
        ID3D12Debug* dc;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
        ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&debug_controller)));
        debug_controller->EnableDebugLayer();
        debug_controller->SetEnableGPUBasedValidation(true);
        dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        dc->Release();
        dc = nullptr;
#endif
        ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));
    }

    void Device::create_adapter()
    {
        for (UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter);
            ++adapterIndex)
        {

            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)) {
                std::string adapter_description = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(desc.Description);

                std::string adapter_info = "Adapter " + std::to_string(adapterIndex) + ": " + adapter_description;
                std::string vram_info = "VRAM: " + std::to_string(desc.DedicatedVideoMemory / (1024 * 1024)) + " MB";
                std::string vendor_info = "VendorId: " + std::to_string(desc.VendorId) + " DeviceId: " + std::to_string(desc.DeviceId);

                WD_INFO(adapter_info);
                WD_INFO(vram_info);
                WD_INFO(vendor_info);

                break;
            }

            // If the adapter doesn't support Direct x12 than we release it
            adapter->Release();
        }
    }

    void Device::create_device()
    {
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&device)));

#if defined(_DEBUG)
        ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&debug_device)));
#endif
    }
}