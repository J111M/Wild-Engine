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

        m_surfaceSize.left = 0;
        m_surfaceSize.top = 0;
        m_surfaceSize.right = static_cast<LONG>(window->get_width());
        m_surfaceSize.bottom = static_cast<LONG>(window->get_height());

        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width = static_cast<float>(window->get_width());
        m_viewport.Height = static_cast<float>(window->get_height());
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
    }

    void Device::Shutdown(){
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_renderTargets[i].reset();
        }

        m_desciptorAllocatorCbvSrvUav.reset();
        m_descriptorAllocatorsDsv.reset();
        m_descriptorAllocatorsRtv.reset();
    }

    void Device::initialize() {
        setup_factory();
        create_adapter();
        create_device();

        m_descriptorAllocatorsRtv = std::make_shared<DescriptorAllocatorRtv>(device, 64);
        m_descriptorAllocatorsDsv = std::make_shared<DescriptorAllocatorDsv>(device, 64);
        m_desciptorAllocatorCbvSrvUav = std::make_shared<DescriptorAllocatorCbvSrvUav>(device, 8192);

        command_queue = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct queue");

        CreateSwapchain();

        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            command_list[i] = std::make_shared<CommandList>(D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
    }

    void Device::begin_frame()
    {
        resize_window();

        current_frame = back_buffer_index;
        back_buffer_index = get_back_buffer_index();

        auto current_command_list = command_list[back_buffer_index];
        auto back_buffer = m_renderTargets[back_buffer_index];

        // Set the rt to the render target state for clearing
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            back_buffer->GetResource().Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        current_command_list->get_list()->ResourceBarrier(1, &barrier);

        FLOAT clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

        current_command_list->get_list()->ClearRenderTargetView(back_buffer->GetRtv()->get_cpu_handle(), clear_color, 0, nullptr);

        current_command_list->BeginRender();
    }

    void Device::end_frame()
    {
        auto current_command_list = command_list[back_buffer_index];
        auto back_buffer = m_renderTargets[back_buffer_index];

        // Present frame
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            back_buffer->GetResource().Get(),
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

        ThrowIfFailed(m_swapchain->Present(sync_interval, flags));

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

            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                m_renderTargets[i].reset();
                m_renderTargets[i] = nullptr;
            }

            CreateSwapchain();

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

            // If the adapter doesn't support DirectX 12 than we release it
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

    void Device::CreateSwapchain()
    {
        if (m_swapchain) {
            m_swapchain->ResizeBuffers(BACK_BUFFER_COUNT, window->get_width(), window->get_height(),
                DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        }
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

            IDXGISwapChain1* new_swapchain;

            ThrowIfFailed(factory->CreateSwapChainForHwnd(command_queue->get_queue().Get(), window->get_handle(), &swapchain_desc, nullptr, nullptr, &new_swapchain), "Swapchain creation failed");

            m_swapchain = reinterpret_cast<IDXGISwapChain4*>(new_swapchain);
        }

        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            CreateTextureFromSwapchain(i);
        }
    }

    void Device::CreateTextureFromSwapchain(UINT index)
    {
        TextureDesc desc;
        DXGI_SWAP_CHAIN_DESC1 scDesc;
        m_swapchain->GetDesc1(&scDesc);
        desc.width = scDesc.Width;
        desc.height = scDesc.Height;

        desc.usage = TextureDesc::gpuOnly;
        desc.flag = TextureDesc::renderTarget;

        ID3D12Resource2* scBuffer = nullptr;
        m_swapchain->GetBuffer(index, IID_PPV_ARGS(&scBuffer));

        m_renderTargets[index] = std::make_shared<Texture>(desc, scBuffer);
    }
}