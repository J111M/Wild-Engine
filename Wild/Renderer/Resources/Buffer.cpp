#include "Renderer/Resources/Buffer.hpp"

namespace Wild {
	Buffer::Buffer(ComPtr<ID3D12Device> device, BufferDesc p_desc)
	{
		desc = p_desc;

		D3D12_RESOURCE_DESC uploadBufferDesc = { D3D12_RESOURCE_DIMENSION_BUFFER,
                                        65536ull,
                                        65536ull,
                                        1u,
                                        1,
                                        1,
                                        DXGI_FORMAT_UNKNOWN,
                                        {1u, 0u},
                                        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                        D3D12_RESOURCE_FLAG_NONE };
	}
}