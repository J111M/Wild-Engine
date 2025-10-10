#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
	struct BufferDesc {
		UINT64 width = 1;
		UINT height = 1;
		UINT16 depth = 1;
		UINT16 mip_levels = 1;
		DXGI_FORMAT format;
		D3D12_RESOURCE_FLAGS flags;
	};

	class Buffer
	{
	public:
		Buffer(ComPtr<ID3D12Device> device, BufferDesc p_desc);
		~Buffer();

	private:

        void* data = nullptr;

        BufferDesc desc;

		ComPtr<ID3D12Resource> buffer;
    public;
        // Vertex buffer view
        constexpr D3D12_VERTEX_BUFFER_VIEW make_vbv() const
        {
            D3D12_VERTEX_BUFFER_VIEW vbv = {};
            vbv.BufferLocation = (UINT64)m_bufferLocation;
            vbv.SizeInBytes = (UINT)Size();
            vbv.StrideInBytes = (UINT)STRIDE;
            return vbv;
        }

        // Index buffer view
        constexpr D3D12_INDEX_BUFFER_VIEW make_ibv() const
        {
            D3D12_INDEX_BUFFER_VIEW ibv = {};
            ibv.BufferLocation = (UINT64)m_bufferLocation;
            ibv.SizeInBytes = (UINT)Size();

            switch (STRIDE)
            {
            case 2:
                ibv.Format = DXGI_FORMAT_R16_UINT;
                break;
            case 4:
                ibv.Format = DXGI_FORMAT_R32_UINT;
                break;
            default:
                throw std::runtime_error("Invalid index buffer stride");
            }

            return ibv;
        }
	};
}