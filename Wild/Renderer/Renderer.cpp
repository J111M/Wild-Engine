#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Core/Camera.hpp"

namespace Wild {
	Renderer::Renderer() {
		auto device = engine.GetDevice();

		//CreateRootSignature();
		//m_settings

		// PSO
		m_vertShader = std::make_shared<Shader>("Shaders/vertShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragShader.hlsl");

		//m_settings.RootSignature = m_rootSignature;
		m_settings.ShaderState.VertexShader = m_vertShader;
		m_settings.ShaderState.FragShader = m_fragShader;
		m_settings.DepthStencilState.DepthEnable = true;

		// Setting up the input layout
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

		std::vector<Uniform> uniforms;

		{
			Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(RootConstant) };
			uniforms.emplace_back(uni);
		}

		{
			Uniform uni{ 0, 0, RootParams::RootResourceType::DescriptorTable };
			CD3DX12_DESCRIPTOR_RANGE srvRange{};
			srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Flag for bindles
			uni.Ranges.emplace_back(srvRange);
			uni.Visibility = D3D12_SHADER_VISIBILITY_PIXEL;

			uniforms.emplace_back(uni);
		}

		{
			Uniform uni{ 0, 0, RootParams::RootResourceType::StaticSampler };
			uniforms.emplace_back(uni);
		}
		
		m_pipeline = std::make_shared<PipelineState>(PipelineStateType::Graphics, m_settings, uniforms);

		auto ecs = engine.GetECS();

		auto entity = ecs->CreateEntity();
		auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
		ecs->AddComponent<Model>(entity, "Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", entity);
		transform.SetPosition(glm::vec3(0, 0, -15));

		m_grassManager = std::make_unique<GrassManager>();
		m_grassPreCompute = std::make_unique<GrassCompute>();

		m_grassPreCompute->Render(*engine.GetDevice()->GetCommandList());

		m_texture = std::make_unique<Texture>("Assets/Models/DamagedHelmet/glTF/Default_albedo.jpg");
	}

	void Renderer::Render(CommandList& list, CommandList& computeList) {
		auto device = engine.GetDevice();

		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(device->GetWidth());
		viewPort.Height = static_cast<FLOAT>(device->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		list.GetList()->SetPipelineState(m_pipeline->GetPso().Get());

		list.GetList()->SetGraphicsRootSignature(m_pipeline->GetRootSignature().Get());

		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		Camera* camera = nullptr;
		for (auto entity : cameras) {
			camera = &ecs->GetComponent<Camera>(entity);
			break;
		}

		list.GetList()->RSSetScissorRects(1, &scissorRect);
		list.GetList()->RSSetViewports(1, &viewPort);
		list.GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set main render target and depth
		list.GetList()->OMSetRenderTargets(1, &device->GetRenderTarget()->GetRtv()->GetCpuHandle(), FALSE, &device->GetDepthTarget()->GetDsv()->GetCpuHandle());
		
		auto meshes = ecs->GetRegistry().view<Transform, Mesh>();
		for (auto&& [entity, trans, mesh] : meshes.each()) {
			if (camera) {
				m_rc.matrix = camera->GetProjection() * camera->GetView() * trans.GetWorldMatrix();
			}

			m_rc.view = m_texture->GetSrv()->View() - 1;

			list.GetList()->SetGraphicsRoot32BitConstants(
				0,
				sizeof(RootConstant) / 4,
				&m_rc,
				0
			);

			ID3D12DescriptorHeap* heaps[] = { engine.GetDevice()->GetCbvSrvUavAllocator()->GetHeap().Get() };
			list.GetList()->SetDescriptorHeaps(1, heaps);

			list.GetList()->SetGraphicsRootDescriptorTable(1, engine.GetDevice()->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());

			list.GetList()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

			if (mesh.HasIndexBuffer()) {
				list.GetList()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
				list.GetList()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
			}
			else {
				list.GetList()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
			}
		}

		// TODO move update to update function
		m_grassManager->Update();

		m_grassManager->Render(list, m_grassPreCompute->GetGrassData());

		/*list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_grassPreCompute->GetGrassData()->GetBuffer().Get(),
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_COMMON
		));*/

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
		ThrowIfFailed(device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pipeline->GetRootSignature())));

		if (error)
		{
			const char* errStr = (const char*)error->GetBufferPointer();
			WD_ERROR(errStr);
		}
	}
}