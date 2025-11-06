#pragma once

#include <dxgi1_6.h>
#include "d3d12.h"

// View abstraction code inspired by https://github.com/t-Boons
namespace Wild {
    class ViewBase : private NonCopyable {
    public:
        const D3D12_GPU_DESCRIPTOR_HANDLE& get_gpu_handle() const { return m_gpuHandle; }
        const D3D12_CPU_DESCRIPTOR_HANDLE& get_cpu_handle() const { return m_cpuHandle; }
        D3D12_GPU_VIRTUAL_ADDRESS get_gpu_virtual_adress() const { return m_resource->GetGPUVirtualAddress(); }

    protected:
        ViewBase(ComPtr<ID3D12Resource2> resource);
        ComPtr<ID3D12Resource2> m_resource;
        D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
        D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
    };

    class RenderTargetView : public ViewBase
    {
    public:
        RenderTargetView(ComPtr<ID3D12Resource2> resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc);
        ~RenderTargetView();

        uint32_t View() const { return m_viewIndex; }

    private:
        D3D12_RENDER_TARGET_VIEW_DESC m_desc;
        uint32_t m_viewIndex;
    };

    class DepthStencilView : public ViewBase
    {
    public:
        DepthStencilView(ComPtr<ID3D12Resource2> resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc);
        ~DepthStencilView();

        uint32_t View() const { return m_viewIndex; }

    private:
        D3D12_DEPTH_STENCIL_VIEW_DESC m_desc;
        uint32_t m_viewIndex;
    };

    class ShaderResourceView : public ViewBase
    {
    public:
        ShaderResourceView(ComPtr<ID3D12Resource2> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
        ~ShaderResourceView();

        uint32_t View() const { return m_viewIndex; }

    private:
        D3D12_SHADER_RESOURCE_VIEW_DESC m_desc;
        uint32_t m_viewIndex;
    };

    class ConstantBufferView : public ViewBase
    {
    public:
        ConstantBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes);
        ~ConstantBufferView();

        uint32_t View() const { return m_viewIndex; }

    private:
        uint32_t m_viewIndex;
        uint32_t m_sizeInBytes;
    };

    class UnorderedAccessView : public ViewBase
    {
    public:
        UnorderedAccessView(ComPtr<ID3D12Resource2> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc);
        ~UnorderedAccessView();

        uint32_t View() const { return m_viewIndex; }

    private:
        D3D12_UNORDERED_ACCESS_VIEW_DESC m_desc;
        uint32_t m_viewIndex;
    };

    //class SamplerView
    //{
    //public:
    //    SamplerView(const D3D12_SAMPLER_DESC& desc);
    //    ~SamplerView();

    //    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle() const { return m_gpuHandle; }
    //    uint32_t View() const { return m_viewIndex; }

    //private:
    //    D3D12_UNORDERED_ACCESS_VIEW_DESC m_desc;
    //    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
    //    uint32_t m_viewIndex;
    //};

    // Index buffer view
    class IndexBufferView : public ViewBase
    {
    public:
        IndexBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes, DXGI_FORMAT format);

        D3D12_INDEX_BUFFER_VIEW View() const { return m_view; }

    private:
        D3D12_INDEX_BUFFER_VIEW m_view;
    };

    // Vertex buffer view
    class VertexBufferView : public ViewBase
    {
    public:
        VertexBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes, uint32_t stride);

        D3D12_VERTEX_BUFFER_VIEW View() const { return m_view; }

    private:
        D3D12_VERTEX_BUFFER_VIEW m_view;
    };
}