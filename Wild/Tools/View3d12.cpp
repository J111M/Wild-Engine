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

}