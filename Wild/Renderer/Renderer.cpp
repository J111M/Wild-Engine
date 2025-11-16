#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Core/Camera.hpp"

namespace Wild {
	Renderer::Renderer() {
		auto device = engine.GetDevice();

		CreateRootSignature();
		//m_settings

		// PSO
		m_vertShader = std::make_shared<Shader>("Shaders/vertShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragShader.hlsl");

		m_settings.RootSignature = m_rootSignature;
		m_settings.ShaderState.VertexShader = m_vertShader;
		m_settings.ShaderState.FragShader = m_fragShader;
		m_settings.DepthStencilState.DepthEnable = true;

		m_pipeline = std::make_shared<PipelineState>(PipelineStateType::Graphics, m_settings);

		//D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		//psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
		//psoDesc.pRootSignature = m_rootSignature.Get();
		//psoDesc.VS = m_vertShader->GetByteCode();
		//psoDesc.PS = m_fragShader->GetByteCode();;
		//psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		//psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		////psoDesc.SampleDesc = sampleDesc;
		//psoDesc.SampleMask = 0xffffffff;
		//psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
		//psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		//psoDesc.NumRenderTargets = 1;
		//psoDesc.SampleDesc.Count = 1;
		//psoDesc.SampleDesc.Quality = 0;

		//psoDesc.DepthStencilState.DepthEnable = TRUE;
		//psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		//psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		//psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		//ThrowIfFailed(device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));

		auto ecs = engine.GetECS();

		auto entity = ecs->CreateEntity();
		auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
		ecs->AddComponent<Model>(entity, "Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", entity);
		transform.SetPosition(glm::vec3(0, 0, -15));
	}

	void Renderer::Render(CommandList& command_list) {
		auto device = engine.GetDevice();

		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(device->GetWidth());
		viewPort.Height = static_cast<FLOAT>(device->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		command_list.GetList()->SetPipelineState(m_pipeline->GetPso().Get());

		command_list.GetList()->SetGraphicsRootSignature(m_rootSignature.Get());

		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		Camera* camera = nullptr;
		for (auto entity : cameras) {
			camera = &ecs->GetComponent<Camera>(entity);
			break;
		}

		command_list.GetList()->RSSetScissorRects(1, &scissorRect);
		command_list.GetList()->RSSetViewports(1, &viewPort);
		command_list.GetList()->RSSetViewports(1, &viewPort);
		command_list.GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set main render target and depth
		command_list.GetList()->OMSetRenderTargets(1, &device->GetRenderTarget()->GetRtv()->get_cpu_handle(), FALSE, &device->GetDepthTarget()->GetDsv()->get_cpu_handle());
		
		auto meshes = ecs->GetRegistry().view<Transform, Mesh>();
		for (auto&& [entity, trans, mesh] : meshes.each()) {
			if (camera) {
				m_rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
			}

			command_list.GetList()->SetGraphicsRoot32BitConstants(
				0,
				sizeof(glm::mat4) / 4,
				&m_rc.matrix,
				0
			);

			command_list.GetList()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

			if (mesh.HasIndexBuffer()) {
				command_list.GetList()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
				command_list.GetList()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
			}
			else {
				command_list.GetList()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
			}
		}		
	}

	void Renderer::CreateRootSignature()
	{
		auto device = engine.GetDevice();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(device->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
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
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.Desc_1_1.NumParameters = 1;
		rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
		rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
		rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
		ThrowIfFailed(device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		m_rootSignature->SetName(L"Wild Root Signature");

		if (error)
		{
			const char* errStr = (const char*)error->GetBufferPointer();
			WD_ERROR(errStr);
		}
	}
}