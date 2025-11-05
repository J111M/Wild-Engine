#include "Renderer/Resources/Buffer.hpp"

namespace Wild {
	Buffer::Buffer(BufferDesc desc)
	{
		// Copy data into buffer function mostly vector data
	}

	void Buffer::create_cpu_resource(BufferDesc desc)
	{
		auto device = engine.get_device();

		device->get_device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(desc.buffer_size),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&buffer));

		buffer->SetName(std::wstring(desc.name.begin(), desc.name.end()).c_str());
	}

	void Buffer::create_gpu_resource(BufferDesc desc)
	{
		auto device = engine.get_device();

		// Upload heap for GPU resources
		ComPtr<ID3D12Resource> upload_heap;

		device->get_device()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(desc.buffer_size), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&upload_heap));

		std::string upload_resource_name = "Upload resource: " + desc.name;

		upload_heap->SetName(std::wstring(upload_resource_name.begin(), upload_resource_name.end()).c_str());

		// Buffer type to upload heap
		D3D12_SUBRESOURCE_DATA data = {};

		if (desc.data) {
			data.pData = reinterpret_cast<BYTE*>(desc.data);
			data.RowPitch = desc.buffer_size;
			data.SlicePitch = desc.buffer_size;
		}

		auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.get_list().Get(), buffer.Get(), upload_heap.Get(), 0, 0, 1, &data);

		list.get_list()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Execute the command list
		list.close();
		device->get_command_queue()->execute_list(list);
		device->get_command_queue()->wait_for_fence();

		vb_view.BufferLocation = buffer->GetGPUVirtualAddress();
		vb_view.StrideInBytes = desc.stride;
		vb_view.SizeInBytes = desc.buffer_size;
	}
}