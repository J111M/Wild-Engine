#include "Renderer/Resources/Buffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"

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

	void Buffer::CreateCpuResource(BufferDesc desc)
	{
		auto device = engine.GetDevice();
		WD_ERROR("CPU resources creation not unimplemented!");
	}

	void Buffer::CreateVertexBuffer(std::vector<Vertex> vertices)
	{
		if (vertices.size() <= 0)
		{
			WD_ERROR("No buffer data supplied at resource creation!");
			return;
		}

		m_desc.stride = sizeof(Vertex);
		m_desc.buffer_size = vertices.size() * sizeof(Vertex);

		if (m_desc.stride == 0) {
			WD_ERROR("No valid stride supplied at resource creation for vertex buffer!");
			return;
		}

		auto device = engine.GetDevice();

		device->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_buffer));

		m_buffer->SetName(std::wstring(m_desc.name.begin(), m_desc.name.end()).c_str());

		// Upload heap for GPU resources
		ComPtr<ID3D12Resource2> upload_heap;

		device->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&upload_heap));

		std::string upload_resource_name = "Upload resource: " + m_desc.name;

		upload_heap->SetName(std::wstring(upload_resource_name.begin(), upload_resource_name.end()).c_str());

		WriteData((void*)vertices.data(), m_desc.buffer_size);

		// Buffer type to upload heap
		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = reinterpret_cast<BYTE*>(m_data);
		data.RowPitch = m_desc.buffer_size;
		data.SlicePitch = m_desc.buffer_size;

		auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.GetList().Get(), m_buffer.Get(), upload_heap.Get(), 0, 0, 1, &data);

		list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// Execute the command list
		list.Close();
		device->GetCommandQueue()->execute_list(list);
		device->GetCommandQueue()->wait_for_fence();

		m_vbView = std::make_shared<VertexBufferView>(m_buffer, m_desc.buffer_size, m_desc.stride);
	}

	void Buffer::CreateConstantBuffer()
	{
		auto device = engine.GetDevice();

		if (m_desc.buffer_size <= 0) {
			WD_ERROR("Constant buffer invalid data size.");
			return;
		}
			
		device->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer((m_desc.buffer_size + 255) & ~255),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_buffer));

		m_cbView = std::make_shared<ConstantBufferView>(m_buffer, m_desc.buffer_size);
	}

	void Buffer::CreateIndexBuffer(std::vector<uint32_t> indices)
	{
		auto device = engine.GetDevice();

		m_desc.buffer_size = indices.size() * sizeof(uint32_t);

		WriteData((void*)indices.data(), m_desc.buffer_size);

		device->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_buffer));

		// Upload buffer
		ComPtr<ID3D12Resource> uploadHeap;
		device->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(m_desc.buffer_size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadHeap));

		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = m_data;
		indexData.RowPitch = m_desc.buffer_size;
		indexData.SlicePitch = indexData.RowPitch;

		auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.GetList().Get(), m_buffer.Get(), uploadHeap.Get(),
			0, 0, 1, &indexData);

		list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

		// Execute the command list
		list.Close();
		device->GetCommandQueue()->execute_list(list);
		device->GetCommandQueue()->wait_for_fence();

		m_ibView = std::make_shared<IndexBufferView>(m_buffer, m_desc.buffer_size, DXGI_FORMAT_R32_UINT);
	}

	void Buffer::Map(ComPtr<ID3D12Resource2> rs)
	{
		m_dataIsMapped = true;
		m_buffer->Map(0, nullptr, &m_data);
	}

	void Buffer::Unmap()
	{
		if (m_dataIsMapped) {
			m_buffer->Unmap(0, nullptr);
			m_dataIsMapped = false;
		}
	}

	void Buffer::WriteData(void* dataSrc, size_t size)
	{
		// Make sure the pointed has allocated data
		m_data = malloc(size);
		m_desc.buffer_size = size;

		if (m_data) {
			memcpy(m_data, dataSrc, size);
		}
	}

	std::shared_ptr<VertexBufferView> Buffer::GetVBView() const
	{
		if (m_vbView) { return m_vbView; }

		WD_WARN("Trying to access invalid vertex buffer view.");
		return nullptr;
	}

	std::shared_ptr<IndexBufferView> Buffer::GetIBView() const
	{
		if (m_ibView) { return m_ibView; }

		WD_WARN("Trying to access invalid index buffer view.");
		return nullptr;
	}

	std::shared_ptr<ConstantBufferView> Buffer::GetCBView() const
	{
		if (m_cbView) { return m_cbView; }

		WD_WARN("Trying to access invalid constant buffer view.");
		return nullptr;
	}
}