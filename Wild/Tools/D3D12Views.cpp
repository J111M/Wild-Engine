#include "Tools/D3D12Views.hpp"

namespace Wild
{
    ViewBase::ViewBase(ComPtr<ID3D12Resource> resource) : m_resource(resource) {}

    namespace
    {
        // Frees a view's descriptor slot
        template <typename GetAllocator> void FreeViewHandle(GetAllocator getAllocator, uint32_t handle)
        {
            auto gfxContext = engine.GetGfxContext();
            if (!gfxContext) return;

            if (auto allocator = ((*gfxContext).*getAllocator)()) allocator->FreeHandle(handle);
        }
    } // namespace

    ///
    /// RenderTargetView
    ///

    RenderTargetView::RenderTargetView(ComPtr<ID3D12Resource> resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc)
        : m_desc(desc), ViewBase(resource)
    {
        m_viewIndex = engine.GetGfxContext()->GetRtvAllocator()->CreateRtv(resource, &desc);
        m_cpuHandle = engine.GetGfxContext()->GetRtvAllocator()->CpuHandle(m_viewIndex);
    }

    RenderTargetView::~RenderTargetView() { FreeViewHandle(&GfxContext::GetRtvAllocator, m_viewIndex); }

    ///
    /// DepthStencilView
    ///

    DepthStencilView::DepthStencilView(ComPtr<ID3D12Resource> resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc)
        : m_desc(desc), ViewBase(resource)
    {
        m_viewIndex = engine.GetGfxContext()->GetDsvAllocator()->CreateDsv(resource, &desc);
        m_cpuHandle = engine.GetGfxContext()->GetDsvAllocator()->CpuHandle(m_viewIndex);
    }

    DepthStencilView::~DepthStencilView() { FreeViewHandle(&GfxContext::GetDsvAllocator, m_viewIndex); }

    ///
    /// ShaderResourceView
    ///

    ShaderResourceView::ShaderResourceView(ComPtr<ID3D12Resource> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
        : m_desc(desc), ViewBase(resource)
    {
        m_viewIndex = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CreateSRV(resource, &desc);
        m_gpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
        m_cpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
    }

    ShaderResourceView::~ShaderResourceView() { FreeViewHandle(&GfxContext::GetCbvSrvUavAllocator, m_viewIndex); }

    ///
    /// ConstantBufferView
    ///

    ConstantBufferView::ConstantBufferView(ComPtr<ID3D12Resource> resource, uint32_t sizeInBytes)
        : m_sizeInBytes(sizeInBytes), ViewBase(resource)
    {
        m_viewIndex = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CreateCBV(sizeInBytes, resource);
        m_gpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
        m_cpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
    }

    ConstantBufferView::~ConstantBufferView() { FreeViewHandle(&GfxContext::GetCbvSrvUavAllocator, m_viewIndex); }

    ///
    /// UnorderedAccessView
    ///

    UnorderedAccessView::UnorderedAccessView(ComPtr<ID3D12Resource> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc)
        : m_desc(desc), ViewBase(resource)
    {
        m_viewIndex = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CreateUAV(resource, &desc);
        m_gpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
        m_cpuHandle = engine.GetGfxContext()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
    }

    UnorderedAccessView::~UnorderedAccessView() { FreeViewHandle(&GfxContext::GetCbvSrvUavAllocator, m_viewIndex); }

    IndexBufferView::IndexBufferView(ComPtr<ID3D12Resource> resource, uint32_t sizeInBytes, DXGI_FORMAT format)
        : ViewBase(resource)
    {
        m_view.BufferLocation = resource->GetGPUVirtualAddress();
        m_view.SizeInBytes = sizeInBytes;
        m_view.Format = format;
    }

    VertexBufferView::VertexBufferView(ComPtr<ID3D12Resource> resource, uint32_t sizeInBytes, uint32_t stride)
        : ViewBase(resource)
    {
        m_view.BufferLocation = resource->GetGPUVirtualAddress();
        m_view.SizeInBytes = sizeInBytes;
        m_view.StrideInBytes = stride;
    }
} // namespace Wild
