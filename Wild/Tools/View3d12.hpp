#pragma once

#include <dxgi1_6.h>
#include "d3d12.h"

// View abstraction code from https://github.com/t-Boons
namespace Wild {
    class D3D12View : private NonCopyable {
    public:
        const D3D12_GPU_DESCRIPTOR_HANDLE& get_gpu_handle() const { return gpu_handle; }
        const D3D12_CPU_DESCRIPTOR_HANDLE& get_cpu_handle() const { return cpu_handle; }
        D3D12_GPU_VIRTUAL_ADDRESS get_gpu_virtual_adress() const;

    protected:
        D3D12View(ComPtr<ID3D12Resource2> p_resource);
        ComPtr<ID3D12Resource2> resource;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
    };

    class D3D12RenderTargetView : public D3D12View
    {
    public:
        D3D12RenderTargetView(ComPtr<ID3D12Resource2> p_resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc);
        ~D3D12RenderTargetView();

        uint32_t View() const { return view_index; }

    private:
        D3D12_RENDER_TARGET_VIEW_DESC m_desc;
        uint32_t view_index;
    };
}