#include "Renderer/CommandList.hpp"

namespace Wild {
	CommandList::CommandList(D3D12_COMMAND_LIST_TYPE list_type)
	{
		auto device = engine.get_device();

		type = list_type;

		ThrowIfFailed(device->get_device()->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
		ThrowIfFailed(device->get_device()->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));

		CreateRootSignature();
	}

	void CommandList::reset() {
		allocator->Reset();
		command_list->Reset(allocator.Get(), nullptr);
	}

	void CommandList::close() {
		if (!command_list_closed) {
			command_list->Close();
			command_list_closed = true;
		}
	}

	void CommandList::BeginRender()
	{
		auto device = engine.get_device();

		root_signature_and_pso = false;
		m_frameInFlight = true;


		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(device->GetWidth());
		viewPort.Height = static_cast<FLOAT>(device->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };
		command_list->RSSetScissorRects(1, &scissorRect);
		command_list->RSSetViewports(1, &viewPort);
		command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//command_list->OMSetRenderTargets(1, handles.data(), false, dsvHandle);

		m_vertShader = std::make_shared<Shader>("Shaders/path");
		m_fragShader = std::make_shared<Shader>("Shaders/path");

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
		psoDesc.pRootSignature = root_signature.Get();
		psoDesc.VS = m_vertShader->GetByteCode();
		psoDesc.PS = m_fragShader->GetByteCode();;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		//psoDesc.SampleDesc = sampleDesc;
		psoDesc.SampleMask = 0xffffffff;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.NumRenderTargets = 1;

		/*D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(psoDesc), &psoDesc
		};
		ThrowIfFailed(device->get_device()->CreateGraphicsPipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pso)));*/

	/*	device->get_command_queue()->execute_list(*this);
		device->get_command_queue()->wait_for_fence();*/
	}

	void CommandList::EndRender()
	{
	}

	void CommandList::CreateRootSignature()
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