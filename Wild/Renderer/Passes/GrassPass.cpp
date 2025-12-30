#include "Renderer/Passes/GrassPass.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

#include <glm/gtc/matrix_access.hpp>

namespace Wild {
	GrassPass::GrassPass(std::shared_ptr<Buffer> GrassBladeBuffer) {
		m_grassBladeInstanceBuffer = GrassBladeBuffer;

		CreateGrassMeshes();

		// Cull data UAV, u0
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(CulledInstance);
			m_culledInstancesBuffer[i] = std::make_shared<Buffer>(desc);
			m_culledInstancesBuffer[i]->CreateUAVBuffer(MAXGRASSBLADES1);
		}

		uint32_t lodAmount = 3;

		// Single uint for writing instance count UAV, u1
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(uint32_t);
			m_instanceCountBuffer[i] = std::make_unique<Buffer>(desc);
			m_instanceCountBuffer[i]->CreateUAVBuffer(lodAmount);
		}

		// Frustum constant buffer
		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(FrustumBuffer);
			m_frustumBuffer = std::make_unique<Buffer>(desc);
			m_frustumBuffer->CreateConstantBuffer();
		}

		// We need to create the commands on the gpu we do that in this buffer
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
			m_drawCommandsBuffer[i] = std::make_unique<Buffer>(desc);
			m_drawCommandsBuffer[i]->CreateUAVBuffer(lodAmount);
		}

		D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};
		argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
		signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
		signatureDesc.NumArgumentDescs = 1;
		signatureDesc.pArgumentDescs = &argumentDesc;


		auto device = engine.GetGfxContext()->GetDevice();
		device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&m_commandSignature));

		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(SceneData);

			for (int i = 0; i < BACK_BUFFER_COUNT; i++)
			{
				m_sceneData[i] = std::make_shared<Buffer>(desc);
				m_sceneData[i]->CreateConstantBuffer();

				// Keep buffer data mapped for cpu write access
				CD3DX12_RANGE readRange(0, 0);
				m_sceneData[i]->Map(&readRange);
			}
		}

		m_chunkEntity = engine.GetECS()->CreateEntity();
		engine.GetECS()->AddComponent<Transform>(m_chunkEntity, glm::vec3(2, 0, -5), m_chunkEntity);
	}

	void GrassPass::Add(Renderer& renderer, RenderGraph& rg)
	{
		AddClearCounterPass(renderer, rg);
		AddGrassCulling(renderer, rg);
		AddRenderGrass(renderer, rg);
	}

	void GrassPass::Update(const float dt)
	{
		UpdateFrustumData();
		auto& device = engine.GetGfxContext();
		auto& ecs = engine.GetECS();

		SceneData SceneCbv{};
		auto& cameras = ecs->View<Camera>();

		// TODO change to support multiple cameras I only have 1 for now
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				SceneCbv.ProjView = cam.GetProjection() * cam.GetView();
				SceneCbv.CameraPosition = cam.GetPosition();
			}
		}

		m_sceneData[device->GetBackBufferIndex()]->WriteData(&SceneCbv);

		m_accumulatedTime += dt * 1;
		m_rc.time = m_accumulatedTime;
	}

	void GrassPass::AddClearCounterPass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<ClearCounterData>();

		rg.AddPass<ClearCounterData>(
			"Clear counter pass",
			PassType::Compute,
			[&renderer, this](ClearCounterData& countData, CommandList& list) {
			PipelineStateSettings settings{};
			settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/computeClearCounter.hlsl");

			std::vector<Uniform> uniforms;
			Uniform uni{ 0, 0, RootParams::RootResourceType::UnorderedAccessView };
			uniforms.emplace_back(uni);

			auto& pipeline = renderer.GetOrCreatePipeline("Clear counter pass", PipelineStateType::Compute, settings, uniforms);
			list.SetPipelineState(pipeline);
			list.BeginRender();

			auto context = engine.GetGfxContext();
			UINT frameIndex = context->GetBackBufferIndex();
			list.GetList()->SetComputeRootUnorderedAccessView(0, m_instanceCountBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());

			list.GetList()->Dispatch(1, 1, 1);
			list.EndRender();

			// TODO abstract resource barriers away
			CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(
				m_instanceCountBuffer[frameIndex]->GetBuffer()
			);
			list.GetList()->ResourceBarrier(1, &uavBarrier);
		});
	}

	void GrassPass::AddGrassCulling(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<GrassCullData>();

		// The dependency makes sure the clear counter pass is executed before executing this pass
		auto dependency = rg.GetPassData<GrassCullData, ClearCounterData>();

		rg.AddPass<GrassCullData>(
			"Grass culling pass",
			PassType::Compute,
			[&renderer, this](GrassCullData& grassData, CommandList& list) {

			auto context = engine.GetGfxContext();

			PipelineStateSettings settings{};
			settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/computeGrassCull.hlsl");

			std::vector<Uniform> uniforms;

			// Per frame frustum data
			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::ConstantBufferView };
				uniforms.emplace_back(uni);
			}

			// Grass instance data
			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::ShaderResourceView };
				uniforms.emplace_back(uni);
			}

			// Culled read write data
			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::UnorderedAccessView };
				uniforms.emplace_back(uni);
			}

			// Byte adress buffer for instance count
			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::UnorderedAccessView };
				uniforms.emplace_back(uni);
			}

			// Draw commands
			{
				Uniform uni{ 2, 0, RootParams::RootResourceType::UnorderedAccessView };
				uniforms.emplace_back(uni);
			}

			auto& pipeline = renderer.GetOrCreatePipeline("Grass culling pass", PipelineStateType::Compute, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender();

			UINT frameIndex = context->GetBackBufferIndex();

			// Set frame data
			list.GetList()->SetComputeRootConstantBufferView(0, m_frustumBuffer->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootShaderResourceView(1, m_grassBladeInstanceBuffer->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(2, m_culledInstancesBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(3, m_instanceCountBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(4, m_drawCommandsBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());

			list.GetList()->Dispatch(((MAXGRASSBLADES1 + 63) / 64), 1, 1);

			list.EndRender();

			m_instanceCountBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
			m_drawCommandsBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
			m_culledInstancesBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			grassData.CulledData = m_culledInstancesBuffer[frameIndex];

		});
	}

	void GrassPass::AddRenderGrass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<RenderGrassData>();
		auto cullPass = rg.GetPassData<RenderGrassData, GrassCullData>();

		auto* deferredData = rg.GetPassData<RenderGrassData, DeferredPassData>();

		passData->FinalTexture = deferredData->FinalTexture;
		passData->DepthTexture = deferredData->DepthTexture;

		rg.AddPass<RenderGrassData>(
			"Grass render pass",
			PassType::Compute,
			[&renderer, cullPass, this](const RenderGrassData& grassData, CommandList& list) {

			PipelineStateSettings settings{};
			settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/vertGrassShader.hlsl");
			settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/fragGrassShader.hlsl");
			settings.DepthStencilState.DepthEnable = true;

			settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
			settings.ShaderState.InputLayout.emplace_back(InputElement("COORDS", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3)));
			settings.ShaderState.InputLayout.emplace_back(InputElement("SWAY", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3) + sizeof(float)));

			settings.RasterizerState.CullMode = CullMode::None;

			std::vector<Uniform> uniforms;
			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(GrassRC) };
				uniforms.emplace_back(uni);
			}

			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::ShaderResourceView, 0, D3D12_SHADER_VISIBILITY_VERTEX };
				uniforms.emplace_back(uni);
			}

			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::ConstantBufferView, 0 };
				uniforms.emplace_back(uni);
			}

			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::ShaderResourceView };
				uniforms.emplace_back(uni);
			}

			auto& pipeline = renderer.GetOrCreatePipeline("Grass render pass", PipelineStateType::Graphics, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender({ grassData.FinalTexture },
				{ ClearOperation::Store },
				grassData.DepthTexture,
				DSClearOperation::Store,
				"Grass Pass");

			auto& gfxContext = engine.GetGfxContext();
			UINT frameIndex = gfxContext->GetBackBufferIndex();

			auto& transform = engine.GetECS()->GetComponent<Transform>(m_chunkEntity);
			m_rc.Matrix = transform.GetWorldMatrix();
			m_rc.bladeId = 0;

			list.GetList()->SetGraphicsRootShaderResourceView(1, m_grassBladeInstanceBuffer->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetGraphicsRootConstantBufferView(2, m_sceneData[gfxContext->GetBackBufferIndex()]->GetBuffer()->GetGPUVirtualAddress());

			if (cullPass->CulledData->GetBuffer()) {
				list.GetList()->SetGraphicsRootShaderResourceView(3, cullPass->CulledData->GetBuffer()->GetGPUVirtualAddress());
			}

			list.GetList()->SetGraphicsRoot32BitConstants(
				0,
				sizeof(GrassRC) / 4,
				&m_rc,
				0
			);

			list.GetList()->IASetVertexBuffers(0, 1, &m_grassVertices->GetVBView()->View());
			list.GetList()->IASetIndexBuffer(&m_grassIndices->GetIBView()->View());

			list.GetList()->ExecuteIndirect(
				m_commandSignature.Get(),
				MAXGRASSBLADES1,
				m_drawCommandsBuffer[frameIndex]->GetBuffer(),
				0,
				m_instanceCountBuffer[frameIndex]->GetBuffer(),
				0);

			list.EndRender();

			renderer.CompositeTexture = grassData.FinalTexture;
		});
	}

	void GrassPass::UpdateFrustumData()
	{
		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		FrustumBuffer FrustumData{};

		// Loop over all camera's currently always chooses the last since there is only 1
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				FrustumData.viewProj = cam.GetView() * cam.GetProjection();
				FrustumData.cameraPos = cam.GetPosition();
			}
		}

		FrustumData.frustumPlanes[0] = glm::row(FrustumData.viewProj, 3) + glm::row(FrustumData.viewProj, 0); // Left
		FrustumData.frustumPlanes[1] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 0); // Right
		FrustumData.frustumPlanes[2] = glm::row(FrustumData.viewProj, 3) + glm::row(FrustumData.viewProj, 1); // Bottom
		FrustumData.frustumPlanes[3] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 1); // Top
		FrustumData.frustumPlanes[4] = glm::row(FrustumData.viewProj, 2); // Near
		FrustumData.frustumPlanes[5] = glm::row(FrustumData.viewProj, 3) - glm::row(FrustumData.viewProj, 2); // Far

		// Normalize frustum planes
		for (int i = 0; i < 6; i++) {
			FrustumData.frustumPlanes[i] = glm::normalize(FrustumData.frustumPlanes[i]);
		}

		// TODO make slider for LOD change in imgui
		FrustumData.lod0 = 15.0f;
		FrustumData.lod1 = 30.0f;
		FrustumData.lod2 = 50.0f;
		FrustumData.maxDistance = 70.0f;
		FrustumData.lodBlendRange = 5.0f;

		m_frustumBuffer->Map();
		m_frustumBuffer->WriteData(&FrustumData);
		m_frustumBuffer->Unmap();
	}

	void GrassPass::CreateGrassMeshes()
	{
		std::vector<GrassVertex> grassVertices{};
		grassVertices.reserve(21);
		std::vector<uint32_t> grassIndices{};
		grassIndices.reserve(45);

		// Grass blade vertice data | position, 1D coordinates and sway
		/// First lod grass blade
		grassVertices.push_back({ {-0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.2f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.2f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.4f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.4f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.6f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.6f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.0f, 0.75f, 0.0f}, 0.0f, 1.0f });

		grassIndices.insert(grassIndices.end(), { 0, 1, 2, 1, 3, 2 }); // First quad
		grassIndices.insert(grassIndices.end(), { 2, 3, 4, 3, 5, 4 }); // Second quad
		grassIndices.insert(grassIndices.end(), { 4, 5, 6, 5, 7, 6 }); // Third quad
		grassIndices.insert(grassIndices.end(), { 6, 7, 8 }); // Top triangle

		/// Second lod grass blade
		grassVertices.push_back({ {-0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.3f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.3f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.6f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.6f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.0f, 0.75f, 0.0f}, 0.0f, 1.0f });

		grassIndices.insert(grassIndices.end(), { 0, 1, 2, 1, 3, 2 }); // First quad
		grassIndices.insert(grassIndices.end(), { 2, 3, 4, 3, 5, 4 }); // Second quad
		grassIndices.insert(grassIndices.end(), { 4, 5, 6 }); // Top triangle

		/// Third lod grass blade 
		grassVertices.push_back({ {-0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.5f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.5f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.0f, 0.75f, 0.0f}, 0.0f, 1.0f });

		grassIndices.insert(grassIndices.end(), { 0, 1, 2, 1, 3, 2 }); // Quad
		grassIndices.insert(grassIndices.end(), { 2, 3, 4 }); // Top triangle

		BufferDesc desc{};
		m_grassVertices = std::make_unique<Buffer>(desc);
		m_grassVertices->CreateVertexBuffer<GrassVertex>(grassVertices);

		m_grassIndices = std::make_unique<Buffer>(desc);
		m_grassIndices->CreateIndexBuffer(grassIndices);
	}
}