#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Core/Camera.hpp"

namespace Wild {
	Renderer::Renderer() {
		auto device = engine.get_device();

		CreateRootSignature();

		// PSO
		m_vertShader = std::make_shared<Shader>("Shaders/vertShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragShader.hlsl");

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3) * 2, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(glm::vec3) * 3, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

		ThrowIfFailed(device->get_device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));

		auto ecs = engine.GetECS();

		auto entity = ecs->CreateEntity();
		auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
		ecs->AddComponent<Model>(entity, "Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", entity);
		transform.SetPosition(glm::vec3(0, 0, -15));
	}

	void Renderer::render(CommandList& command_list) {
		auto device = engine.get_device();

		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(device->GetWidth());
		viewPort.Height = static_cast<FLOAT>(device->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		command_list.get_list()->SetPipelineState(m_pso.Get());

		command_list.get_list()->SetGraphicsRootSignature(root_signature.Get());

		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		Camera* camera = nullptr;
		for (auto entity : cameras) {
			camera = &ecs->GetComponent<Camera>(entity);
			break;
		}

		command_list.get_list()->RSSetScissorRects(1, &scissorRect);
		command_list.get_list()->RSSetViewports(1, &viewPort);
		command_list.get_list()->RSSetViewports(1, &viewPort);
		command_list.get_list()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		command_list.get_list()->OMSetRenderTargets(1, &device->GetRenderTarget()->GetRtv()->get_cpu_handle(), false, nullptr);

		auto meshes = ecs->GetRegistry().view<Transform, Mesh>();
		for (auto&& [entity, trans, mesh] : meshes.each()) {
			if (camera) {
				m_rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
			}

			command_list.get_list()->SetGraphicsRoot32BitConstants(
				0,
				sizeof(glm::mat4) / 4,
				&m_rc.matrix,
				0
			);

			command_list.get_list()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

			if (mesh.HasIndexBuffer()) {
				command_list.get_list()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
				command_list.get_list()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
			}
			else {
				command_list.get_list()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
			}
			
		}		
	}

	void Renderer::CreateRootSignature()
	{
		auto device = engine.get_device();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->get_device()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

		//// GPU resources
		//D3D12_DESCRIPTOR_RANGE1 ranges[1];
		//ranges[0].BaseShaderRegister = 0;
		//ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		//ranges[0].NumDescriptors = 1;
		//ranges[0].RegisterSpace = 0;
		//ranges[0].OffsetInDescriptorsFromTableStart = 0;
		//ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

		CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};
		rootParameters[0].InitAsConstants(
			16,
			0,
			0
		);

		// Root signature layout
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
		root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		root_signature_desc.Desc_1_1.NumParameters = 1;
		root_signature_desc.Desc_1_1.pParameters = rootParameters;
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