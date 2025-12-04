#pragma once

#include "Tools/Common3d12.hpp"
#include "Tools/View3d12.hpp"

#include <string>
#include <vector>

namespace Wild {
    struct Vertex;

	struct BufferDesc {
        UINT64 width{ 1 };
        UINT height{ 1 };
        UINT16 depth{ 1 };
        UINT16 mip_levels{ 1 };
		DXGI_FORMAT format;
		D3D12_RESOURCE_FLAGS flags;

        int bufferSize{};
        UINT stride{};

        std::string name = "default";
	};

    
	class Buffer
	{
	public:

		Buffer(BufferDesc desc);
		~Buffer();

        void CreateCpuResource(BufferDesc desc);

        void CreateConstantBuffer();
		void CreateUAVBuffer(uint32_t numElements);
        void CreateIndexBuffer(std::vector<uint32_t> indices);

        void Map(CD3DX12_RANGE* readRange = nullptr);
        void Unmap();

        void WriteData(void* dataSrc, size_t size = 0);

		ComPtr<ID3D12Resource2> GetBuffer() { return m_buffer; }

        std::shared_ptr<VertexBufferView> GetVBView() const;
        std::shared_ptr<IndexBufferView> GetIBView() const;
        std::shared_ptr<ConstantBufferView> GetCBView() const;
        std::shared_ptr<UnorderedAccessView> GetUAView() const;
	private:
        BufferDesc m_desc;

		ComPtr<ID3D12Resource2> m_buffer;

        bool m_dataIsMapped = false;

        // Vertex buffer view
        std::shared_ptr<VertexBufferView> m_vbView;
        std::shared_ptr<IndexBufferView> m_ibView;
        std::shared_ptr<ConstantBufferView> m_cbView;
        std::shared_ptr<UnorderedAccessView> m_uaView;

        void* m_data = nullptr;

    public:
		template<typename T>
		void CreateVertexBuffer(const std::vector<T>& vertices)
		{
			if (vertices.size() <= 0)
			{
				WD_ERROR("No buffer data supplied at resource creation!");
				return;
			}

			m_desc.stride = sizeof(T);
			m_desc.bufferSize = vertices.size() * sizeof(T);

			if (m_desc.stride == 0) {
				WD_ERROR("No valid stride supplied at resource creation for vertex buffer!");
				return;
			}

			auto device = engine.GetDevice();

			device->GetDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize),
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&m_buffer));

			m_buffer->SetName(std::wstring(m_desc.name.begin(), m_desc.name.end()).c_str());

			// Upload heap for GPU resources
			ComPtr<ID3D12Resource2> upload_heap;

			device->GetDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
				D3D12_HEAP_FLAG_NONE, // no flags
				&CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize), // resource description for a buffer
				D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
				nullptr,
				IID_PPV_ARGS(&upload_heap));

			std::string upload_resource_name = "Upload resource: " + m_desc.name;

			upload_heap->SetName(std::wstring(upload_resource_name.begin(), upload_resource_name.end()).c_str());

			WriteData((void*)vertices.data(), m_desc.bufferSize);

			// Buffer type to upload heap
			D3D12_SUBRESOURCE_DATA data = {};
			data.pData = reinterpret_cast<BYTE*>(m_data);
			data.RowPitch = m_desc.bufferSize;
			data.SlicePitch = m_desc.bufferSize;

			auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

			UpdateSubresources(list.GetList().Get(), m_buffer.Get(), upload_heap.Get(), 0, 0, 1, &data);

			list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

			// Execute the command list
			list.Close();
			device->GetCommandQueue(QueueType::Direct)->execute_list(list);
			device->GetCommandQueue(QueueType::Direct)->wait_for_fence();

			m_vbView = std::make_shared<VertexBufferView>(m_buffer, m_desc.bufferSize, m_desc.stride);
		}
	};
}