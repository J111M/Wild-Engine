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

        int buffer_size{};
        UINT stride{};

        std::string name = "default";
	};

    
	class Buffer
	{
	public:

		Buffer(BufferDesc desc);
		~Buffer();

        void create_cpu_resource(BufferDesc desc);

        void create_vertex_buffer(std::vector<Vertex> vertices);
        void CreateConstantBuffer();
        void CreateIndexBuffer(std::vector<uint32_t> indices);

        void Map();
        void Unmap();

        void WriteData(void* dataSrc, size_t size);

        std::shared_ptr<VertexBufferView> GetVBView() const;
        std::shared_ptr<IndexBufferView> GetIBView() const;
        std::shared_ptr<ConstantBufferView> GetCBView() const;
	private:
        BufferDesc m_desc;

		ComPtr<ID3D12Resource2> buffer;

        bool m_dataIsMapped = false;

        // Vertex buffer view
        std::shared_ptr<VertexBufferView> m_vbView;
        std::shared_ptr<IndexBufferView> m_ibView;
        std::shared_ptr<ConstantBufferView> m_cbView;

        void* m_data = nullptr;

        // Vertex buffer view
        //constexpr D3D12_VERTEX_BUFFER_VIEW make_vbv() const
        //{
        //    D3D12_VERTEX_BUFFER_VIEW vbv = {};
        //    vbv.BufferLocation = (UINT64)m_bufferLocation;
        //    vbv.SizeInBytes = (UINT)Size();
        //    vbv.StrideInBytes = (UINT)STRIDE;
        //    return vbv;
        //}

        //// Index buffer view
        //constexpr D3D12_INDEX_BUFFER_VIEW make_ibv() const
        //{
        //    D3D12_INDEX_BUFFER_VIEW ibv = {};
        //    ibv.BufferLocation = (UINT64)m_bufferLocation;
        //    ibv.SizeInBytes = (UINT)Size();

        //    switch (STRIDE)
        //    {
        //    case 2:
        //        ibv.Format = DXGI_FORMAT_R16_UINT;
        //        break;
        //    case 4:
        //        ibv.Format = DXGI_FORMAT_R32_UINT;
        //        break;
        //    default:
        //        throw std::runtime_error("Invalid index buffer stride");
        //    }

        //    return ibv;
        //}
	};
}