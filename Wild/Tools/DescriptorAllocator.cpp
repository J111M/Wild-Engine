#include "Tools/DescriptorAllocator.hpp"

namespace Wild {
	void DescriptorAllocator::FreeHandle(uint32_t handle)
	{
		if (handle >= m_nextFreeIndex)
			WD_ERROR("Handle is higher than the existing index.");
		IFCHECK(std::find(m_freedDiscriptors.begin(), m_freedDiscriptors.end(), handle) == m_freedDiscriptors.end(), "Handle is higher than existing index")

		m_freedDiscriptors.push_back(handle);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GpuHandle(uint32_t handle) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += m_incrementSize * handle;
		return gpuHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::CpuHandle(uint32_t handle) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * handle;
		return cpuHandle;
	}

	DescriptorAllocator::DescriptorAllocator(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_DESC description, const std::wstring& resourceName) : m_desc(description), m_incrementSize(device->GetDescriptorHandleIncrementSize(description.Type))
	{
		ThrowIfFailed(device->CreateDescriptorHeap(&m_desc, IID_PPV_ARGS(&m_heap)));
		m_heap->SetName(resourceName.c_str());
		m_nextFreeIndex = 1;
		m_freedDiscriptors.reserve(64);
	}

	uint32_t DescriptorAllocator::NextFreeHandle()
	{
		if (!m_freedDiscriptors.empty())
		{
			const uint32_t firstFreedValue = *m_freedDiscriptors.begin();
			m_freedDiscriptors.erase(m_freedDiscriptors.begin());
			return firstFreedValue + 1;
		}

		return m_nextFreeIndex++;
	}

	/// Render target view allocator

	DescriptorAllocatorRtv::DescriptorAllocatorRtv(ComPtr<ID3D12Device> device, const uint32_t numDescriptors) : DescriptorAllocator(
		device,
		D3D12_DESCRIPTOR_HEAP_DESC{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 },
		L"Render target view Heap")
	{
	}

	uint32_t DescriptorAllocatorRtv::CreateRtv(const ComPtr<ID3D12Resource2> resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc)
	{
		if (m_nextFreeIndex > static_cast<uint32_t>(m_desc.NumDescriptors))
			WD_FATAL("Rtv heap has overflown.");

		const uint32_t nextFreeValue = NextFreeHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * nextFreeValue;
		engine.GetDevice()->GetDevice()->CreateRenderTargetView(resource.Get(), desc, cpuHandle);

		return nextFreeValue;
	}

	/// Depth stencil view

	DescriptorAllocatorDsv::DescriptorAllocatorDsv(ComPtr<ID3D12Device> device, const uint32_t numDescriptors) : DescriptorAllocator(
		device,
		D3D12_DESCRIPTOR_HEAP_DESC{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 },
		L"Depth stencil view Heap")
	{
	}

	uint32_t DescriptorAllocatorDsv::CreateDsv(const ComPtr<ID3D12Resource2> resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
	{
		if (m_nextFreeIndex > static_cast<uint32_t>(m_desc.NumDescriptors))
			WD_FATAL("Rtv heap has overflown.");

		const uint32_t nextFreeValue = NextFreeHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * nextFreeValue;
		engine.GetDevice()->GetDevice()->CreateDepthStencilView(resource.Get(), desc, cpuHandle);

		return nextFreeValue;
	}


	DescriptorAllocatorCbvSrvUav::DescriptorAllocatorCbvSrvUav(ComPtr<ID3D12Device> device, const uint32_t numDescriptors) : DescriptorAllocator(device,
		D3D12_DESCRIPTOR_HEAP_DESC{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
								   numDescriptors,
								   D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
								   0 },
		L"CbvSrvUav allocator Heap")
	{
	}

	uint32_t DescriptorAllocatorCbvSrvUav::CreateCBV(uint32_t numBytes, const ComPtr<ID3D12Resource2> resource)
	{
		if (m_nextFreeIndex > static_cast<uint32_t>(m_desc.NumDescriptors))
			WD_FATAL("Heap has overflown.");

		IFCHECK(numBytes % 256 == 0, "Constant buffer views should always have a 256 byte alignment.");

		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.SizeInBytes = numBytes;
		desc.BufferLocation	= resource->GetGPUVirtualAddress();

		// Get the next free handle in the descriptor heap
		const uint32_t nextFreeHandle = NextFreeHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * nextFreeHandle;
		engine.GetDevice()->GetDevice()->CreateConstantBufferView(&desc, cpuHandle);

		return nextFreeHandle;
	}

	uint32_t DescriptorAllocatorCbvSrvUav::CreateSRV(const ComPtr<ID3D12Resource2> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
	{
		if (m_nextFreeIndex > static_cast<uint32_t>(m_desc.NumDescriptors))
			WD_FATAL("Heap has overflown.");

		const uint32_t nextFreeHandle = NextFreeHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * nextFreeHandle;
		engine.GetDevice()->GetDevice()->CreateShaderResourceView(resource.Get(), desc, cpuHandle);


		return nextFreeHandle;
	}

	uint32_t DescriptorAllocatorCbvSrvUav::CreateUAV(const ComPtr<ID3D12Resource2> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
	{
		if (m_nextFreeIndex > static_cast<uint32_t>(m_desc.NumDescriptors))
			WD_FATAL("Heap has overflown.");

		const uint32_t nextFreeHandle = NextFreeHandle();

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += m_incrementSize * nextFreeHandle;
		engine.GetDevice()->GetDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, desc, cpuHandle);

		return nextFreeHandle;
	}

}