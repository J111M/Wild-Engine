#include "Renderer/Device.hpp"

#include <initguid.h>
#include <codecvt>
#include <locale>

namespace Wild {
    Device::Device(std::shared_ptr<Window> window)
    {
        setup_factory();
        create_adapter();
        create_device();

        cmd_queue = std::make_shared<CommandQueue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct queue");
        swapchain = std::make_unique<Swapchain>(window, *this);
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