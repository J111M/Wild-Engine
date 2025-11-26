#pragma once

#include <d3d12.h>

namespace Wild {
	struct InputElement : public D3D12_INPUT_ELEMENT_DESC
	{
        InputElement(LPCSTR name, DXGI_FORMAT format, UINT offset = D3D12_APPEND_ALIGNED_ELEMENT, UINT semanticIndex = 0, UINT slot = 0)
        {
            SemanticName = name;
            SemanticIndex = semanticIndex;
            Format = format;
            InputSlot = slot;
            AlignedByteOffset = offset;
            InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            InstanceDataStepRate = 0;
        }
	};
}