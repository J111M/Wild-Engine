#include "Tools/View3d12.hpp"

namespace Wild {
	D3D12_GPU_VIRTUAL_ADDRESS D3D12View::get_gpu_virtual_adress() const
	{
		return resource->GetGPUVirtualAddress();
	}

	D3D12View::D3D12View(ComPtr<ID3D12Resource2> p_resource) : resource(p_resource) {}

	///
	/// D3D12RenderTargetView
	///

	D3D12RenderTargetView::D3D12RenderTargetView(ComPtr<ID3D12Resource2> p_resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc) : m_desc(desc), D3D12View(resource)
	{
	/*	view_index = engine.get_device().DescriptorAllocatorRtv()->CreateRtv(resource, &desc);
		cpu_handle = engine.get_device().DescriptorAllocatorRtv()->CpuHandle(view_index);*/
	}
}