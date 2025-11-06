#include "Tools/View3d12.hpp"

namespace Wild {
	ViewBase::ViewBase(ComPtr<ID3D12Resource2> resource) : m_resource(resource) {}

	///
	/// RenderTargetView
	///

	RenderTargetView::RenderTargetView(ComPtr<ID3D12Resource2> resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc) : m_desc(desc), ViewBase(resource)
	{
		m_viewIndex = engine.get_device()->GetRtvAllocator()->CreateRtv(resource, &desc);
		m_cpuHandle = engine.get_device()->GetRtvAllocator()->CpuHandle(m_viewIndex);
	}

	RenderTargetView::~RenderTargetView()
	{
		engine.get_device()->GetRtvAllocator()->FreeHandle(m_viewIndex);
	}

	///
	/// DepthStencilView
	///

	DepthStencilView::DepthStencilView(ComPtr<ID3D12Resource2> resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc) : m_desc(desc), ViewBase(resource)
	{
		m_viewIndex = engine.get_device()->GetDsvAllocator()->CreateDsv(resource, &desc);
		m_cpuHandle = engine.get_device()->GetDsvAllocator()->CpuHandle(m_viewIndex);
	}

	DepthStencilView::~DepthStencilView()
	{
		engine.get_device()->GetDsvAllocator()->FreeHandle(m_viewIndex);
	}

	///
	/// ShaderResourceView
	///

	ShaderResourceView::ShaderResourceView(ComPtr<ID3D12Resource2> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc) : m_desc(desc), ViewBase(resource)
	{
		m_viewIndex = engine.get_device()->GetCbvSrvUavAllocator()->CreateSRV(resource, &desc);
		m_gpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
		m_cpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
	}

	ShaderResourceView::~ShaderResourceView()
	{
		engine.get_device()->GetCbvSrvUavAllocator()->FreeHandle(m_viewIndex);
	}

	///
	/// ConstantBufferView
	///

	ConstantBufferView::ConstantBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes) : m_sizeInBytes(sizeInBytes), ViewBase(resource)
	{
		m_viewIndex = engine.get_device()->GetCbvSrvUavAllocator()->CreateCBV(sizeInBytes, resource);
		m_gpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
		m_cpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
	}

	ConstantBufferView::~ConstantBufferView()
	{
		engine.get_device()->GetCbvSrvUavAllocator()->FreeHandle(m_viewIndex);
	}

	///
	/// UnorderedAccessView
	///

	UnorderedAccessView::UnorderedAccessView(ComPtr<ID3D12Resource2> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc) : m_desc(desc), ViewBase(resource)
	{
		m_viewIndex = engine.get_device()->GetCbvSrvUavAllocator()->CreateUAV(resource, &desc);
		m_gpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->GpuHandle(m_viewIndex);
		m_cpuHandle = engine.get_device()->GetCbvSrvUavAllocator()->CpuHandle(m_viewIndex);
	}

	UnorderedAccessView::~UnorderedAccessView()
	{
		engine.get_device()->GetCbvSrvUavAllocator()->FreeHandle(m_viewIndex);
	}

	IndexBufferView::IndexBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes, DXGI_FORMAT format) : ViewBase(resource)
	{
		m_view.BufferLocation = resource->GetGPUVirtualAddress();
		m_view.SizeInBytes = sizeInBytes;
		m_view.Format = format;
	}

	VertexBufferView::VertexBufferView(ComPtr<ID3D12Resource2> resource, uint32_t sizeInBytes, uint32_t stride) : ViewBase(resource)
	{
		m_view.BufferLocation = resource->GetGPUVirtualAddress();
		m_view.SizeInBytes = sizeInBytes;
		m_view.StrideInBytes = stride;
	}
}