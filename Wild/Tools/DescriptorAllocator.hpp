#pragma once

#include "Tools/Common3d12.hpp"

#include <vector>

// View abstraction code inspired by https://github.com/t-Boons
namespace Wild {
	class DescriptorAllocator : private NonCopyable
	{
	public:
		void FreeHandle(uint32_t handle);
		ComPtr<ID3D12DescriptorHeap> GetHeap() const { return m_heap; }

		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t handle) const;
		D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t handle) const;

		const uint32_t GetIncrementSize() const { return m_incrementSize; }
		
		~DescriptorAllocator() {};
	protected:
		DescriptorAllocator(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_DESC description, const std::wstring& resourceName = L"default");
		uint32_t NextFreeHandle();

		const D3D12_DESCRIPTOR_HEAP_DESC m_desc;
		uint32_t m_incrementSize{};
		ComPtr<ID3D12DescriptorHeap> m_heap;

		uint32_t m_nextFreeIndex{};
		std::vector<uint32_t> m_freedDiscriptors{};
	};

	class DescriptorAllocatorRtv : public DescriptorAllocator
	{
	public:
		DescriptorAllocatorRtv(ComPtr<ID3D12Device> device, const uint32_t numDescriptors);

		uint32_t CreateRtv(const ComPtr<ID3D12Resource2> resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc);
	};

	class DescriptorAllocatorDsv : public DescriptorAllocator
	{
	public:
		DescriptorAllocatorDsv(ComPtr<ID3D12Device> device, const uint32_t numDescriptors);

		uint32_t CreateDsv(const ComPtr<ID3D12Resource2> resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc);
	};
}