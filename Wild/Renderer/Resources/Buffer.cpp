#include "Renderer/Resources/Buffer.hpp"

namespace Wild {
	Buffer::Buffer(BufferDesc desc)
	{
		m_desc = desc;
	}

	Buffer::~Buffer()
	{
		if (m_data) {
			free(m_data);
			m_data = nullptr;
		}
	}

	void Buffer::create_cpu_resource(BufferDesc desc)
	{
		auto device = engine.get_device();
		WD_ERROR("CPU resources creation not unimplemented!");
	}

	void Buffer::create_gpu_resource()
	{
		if (!m_data) {
			WD_ERROR("No buffer data supplied at resource creation!");
			return;
		}

		if (m_desc.stride == 0) {
			WD_ERROR("No stride supplied at resource creation for vertex buffer!");
			return;
		}

		auto device = engine.get_device();

		device->get_device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&buffer));

		buffer->SetName(std::wstring(m_desc.name.begin(), m_desc.name.end()).c_str());

		// Upload heap for GPU resources
		ComPtr<ID3D12Resource> upload_heap;

		device->get_device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&upload_heap));

		std::string upload_resource_name = "Upload resource: " + m_desc.name;

		upload_heap->SetName(std::wstring(upload_resource_name.begin(), upload_resource_name.end()).c_str());

		// Buffer type to upload heap
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = reinterpret_cast<BYTE*>(m_data);
		data.RowPitch = m_desc.buffer_size;
		data.SlicePitch = m_desc.buffer_size;

		auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.get_list().Get(), buffer.Get(), upload_heap.Get(), 0, 0, 1, &data);

		list.get_list()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Execute the command list
		list.close();
		device->get_command_queue()->execute_list(list);
		device->get_command_queue()->wait_for_fence();

		vb_view.BufferLocation = buffer->GetGPUVirtualAddress();
		vb_view.StrideInBytes = m_desc.stride;
		vb_view.SizeInBytes = m_desc.buffer_size;
	}

	void Buffer::WriteData(void* data, size_t size)
	{
		m_data = malloc(size);
		m_desc.buffer_size = size;
		memcpy(m_data, data, size);
	}
}