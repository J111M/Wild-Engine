#include "Renderer/Device.hpp"

#include <initguid.h>
#include <codecvt>
#include <locale>

namespace Wild {
    Device::Device(Window& window)
    {
        setup_factory();
        create_adapter();
        create_device();
    }

    void Device::setup_factory()
    {
#if defined(_DEBUG)
        
        ID3D12Debug* dc;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
        ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&m_debug_controller)));
        m_debug_controller->EnableDebugLayer();
        m_debug_controller->SetEnableGPUBasedValidation(true);
        m_dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        dc->Release();
        dc = nullptr;
#endif
        ThrowIfFailed(CreateDXGIFactory2(m_dxgi_factory_flags, IID_PPV_ARGS(&m_factory)));
    }

    void Device::create_adapter()
    {
        for (UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &m_adapter);
            ++adapterIndex)
        {

            DXGI_ADAPTER_DESC1 desc;
            m_adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)) {
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
            m_adapter->Release();
        }
    }

    void Device::create_device()
    {
        ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device)));

#if defined(_DEBUG)
        ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&m_debugDevice)));
#endif
    }
}