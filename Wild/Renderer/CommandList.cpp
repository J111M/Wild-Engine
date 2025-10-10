#include "Renderer/CommandList.hpp"

namespace Wild {
	CommandList::CommandList(ComPtr<ID3D12Device4> device, D3D12_COMMAND_LIST_TYPE list_type)
	{
		type = list_type;

		ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
		ThrowIfFailed(device->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));
	}

	void CommandList::create_root_signature()
	{
		auto device = engine.get_device();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->get_device()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

		// GPU resources
		D3D12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].BaseShaderRegister = 0;
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[0].NumDescriptors = 1;
		ranges[0].RegisterSpace = 0;
		ranges[0].OffsetInDescriptorsFromTableStart = 0;
		ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

		D3D12_ROOT_PARAMETER1 root_parameters[1];
		root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		root_parameters[0].DescriptorTable.NumDescriptorRanges = 1;
		root_parameters[0].DescriptorTable.pDescriptorRanges = ranges;

		// Root signature layout
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
		root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		root_signature_desc.Desc_1_1.NumParameters = 1;
		root_signature_desc.Desc_1_1.pParameters = root_parameters;
		root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
		root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
		ThrowIfFailed(device->get_device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
		root_signature->SetName(L"Wild Root Signature");

		if (error)
		{
			const char* errStr = (const char*)error->GetBufferPointer();
			WD_ERROR(errStr);
		}
	}
}