#include "Renderer/Passes/GrassPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"

#include <glm/gtc/matrix_access.hpp>

namespace Wild {
	GrassPass::GrassPass(std::shared_ptr<Buffer> GrassBladeBuffer) {
		m_grassBladeInstanceBuffer = GrassBladeBuffer;

		// Generate grass mesh data
		CreateGrassMeshes();

		// Cull data UAV, u0
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			BufferDesc desc{};
			desc.name = "Cullinstance buffer: " + std::to_string(i);
			desc.bufferSize = sizeof(CulledInstance);
			m_culledInstancesBuffer[i] = std::make_shared<Buffer>(desc);
			m_culledInstancesBuffer[i]->CreateUAVBuffer(MAXGRASSBLADES1);
		}

		// Single uint for writing instance count UAV, u1
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			BufferDesc desc{};
			desc.name = "Indirect instance count buffer: " + std::to_string(i);
			desc.bufferSize = sizeof(uint32_t);
			m_instanceCountBuffer[i] = std::make_unique<Buffer>(desc);
			m_instanceCountBuffer[i]->CreateUAVBuffer(m_lodAmount);
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
			desc.name = "Indirect commands buffer: " + std::to_string(i);
			desc.bufferSize = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
			m_drawCommandsBuffer[i] = std::make_unique<Buffer>(desc);
			m_drawCommandsBuffer[i]->CreateUAVBuffer(m_lodAmount);
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
		// Adds each pass to the render graph
		AddClearCounterPass(renderer, rg);
		AddGrassCulling(renderer, rg);
		AddIndirectDrawCommandsPass(renderer, rg);
		AddRenderGrass(renderer, rg);
	}

	void GrassPass::Update(const float dt)
	{
		UpdateFrustumData();
		auto& device = engine.GetGfxContext();
		auto& ecs = engine.GetECS();

		ImGui::Begin("Wind Settings");

		ImGui::SliderFloat("Wind Strength", &m_grassSceneData.windStrength, 0.01f, 20.0f);
		ImGui::SliderFloat("Octaves", &m_grassSceneData.octaves, 0.1, 1);
		ImGui::SliderFloat("Frequency", &m_grassSceneData.frequency, 0.01f, 0.4f);
		ImGui::SliderFloat("Amplitude", &m_grassSceneData.amplitude, 0.01f, 1.0f);
		ImGui::SliderFloat2("Wind Direction", &m_grassSceneData.windDirection.x, -10.0f, 10.0f);

		ImGui::End();

		SceneData SceneCbv{};
		auto& cameras = ecs->View<Camera>();

		// TODO change to support multiple cameras I only have 1 for now
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				SceneCbv.ProjView = cam.GetProjection() * cam.GetView();
				SceneCbv.CameraPosition = cam.GetPosition();

				SceneCbv.windStrength = m_grassSceneData.windStrength;
				SceneCbv.octaves = m_grassSceneData.octaves;
				SceneCbv.frequency = m_grassSceneData.frequency;
				SceneCbv.amplitude = m_grassSceneData.amplitude;
				SceneCbv.windDirection = m_grassSceneData.windDirection;
			}
		}

		m_sceneData[device->GetBackBufferIndex()]->Map();
		m_sceneData[device->GetBackBufferIndex()]->WriteData(&SceneCbv);
		m_sceneData[device->GetBackBufferIndex()]->Unmap();

		m_accumulatedTime += dt * 1;
		m_rc.time = m_accumulatedTime;
	}

	/// <summary>
	/// The reason I clear the instance count via a compute shader is because doing it via clear uav uint had specific
	/// requirments for how the heap should be created which required me to create another heap specifically for clearing
	/// compute made it more straight forward without cluttering the code.
	/// </summary>
	void GrassPass::AddClearCounterPass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<ClearCounterData>();

		rg.AddPass<ClearCounterData>(
			"Clear counter pass",
			PassType::Compute,
			[&renderer, this](ClearCounterData& countData, CommandList& list) {
			PipelineStateSettings settings{};
			settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeClearCounter.slang");

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

	/// <summary>
	/// GPU frustum culling combined with indirect rendering.
	/// This pass also calculates which LOD's to use for the grass blades
	/// and writes the amount of instances to the instance count buffer
	/// which will be used in the AddIndirectDrawCommandsPass.
	/// </summary>
	void GrassPass::AddGrassCulling(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<GrassCullData>();

		// The dependency makes sure the clear counter pass is executed before executing this pass
		auto dependency = rg.GetPassData<GrassCullData, ClearCounterData>();

		rg.AddPass<GrassCullData>(
			"Grass culling pass",
			PassType::Compute,
			[&renderer, this](GrassCullData& cullingData, CommandList& list) {

			auto context = engine.GetGfxContext();

			PipelineStateSettings settings{};
			settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeGrassCulling.slang");

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

			auto& pipeline = renderer.GetOrCreatePipeline("Grass culling pass", PipelineStateType::Compute, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender();

			UINT frameIndex = context->GetBackBufferIndex();

			// Set frame data
			list.GetList()->SetComputeRootConstantBufferView(0, m_frustumBuffer->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootShaderResourceView(1, m_grassBladeInstanceBuffer->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(2, m_culledInstancesBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(3, m_instanceCountBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());

			list.GetList()->Dispatch(((MAXGRASSBLADES1 + 63) / 64), 1, 1);

			list.EndRender();

			D3D12_RESOURCE_BARRIER barriers[1] = {};

			// Barrier for instance count buffer
			barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[0].UAV.pResource = m_instanceCountBuffer[frameIndex]->GetBuffer();

			list.GetList()->ResourceBarrier(1, barriers);

			m_culledInstancesBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			cullingData.CulledBuffer = m_culledInstancesBuffer[frameIndex];

		});
	}

	/// <summary>
	/// Creates N amount of draw commands depending on the amount of LOD's the grass blades.
	/// </summary>
	void GrassPass::AddIndirectDrawCommandsPass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<IndirectCommandsData>();
		auto cullData = rg.GetPassData<IndirectCommandsData, GrassCullData>();

		rg.AddPass<IndirectCommandsData>("Indirect command creation pass",
			PassType::Compute,
			[&renderer, this](const IndirectCommandsData& indirectCmds, CommandList& list) {
			auto context = engine.GetGfxContext();

			PipelineStateSettings settings{};
			settings.ShaderState.ComputeShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/ComputeIndirectCommands.slang");

			std::vector<Uniform> uniforms;
			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::UnorderedAccessView };
				uniforms.emplace_back(uni);
			}
			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::UnorderedAccessView };
				uniforms.emplace_back(uni);
			}

			auto& pipeline = renderer.GetOrCreatePipeline("Indirect command creation pass", PipelineStateType::Compute, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender();

			int frameIndex = context->GetBackBufferIndex();
			list.GetList()->SetComputeRootUnorderedAccessView(0, m_instanceCountBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());
			list.GetList()->SetComputeRootUnorderedAccessView(1, m_drawCommandsBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());

			list.GetList()->Dispatch(1, 1, 1);

			list.EndRender();

			m_instanceCountBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
			m_drawCommandsBuffer[frameIndex]->Transition(list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		});
	}

	void GrassPass::AddRenderGrass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<RenderGrassData>();
		auto indirectPass = rg.GetPassData<RenderGrassData, IndirectCommandsData>();

		auto* pbrData = rg.GetPassData<RenderGrassData, PbrPassData>();

		passData->FinalTexture = pbrData->FinalTexture;
		passData->DepthTexture = pbrData->DepthTexture;

		rg.AddPass<RenderGrassData>(
			"Grass render pass",
			PassType::Compute,
		[&renderer, this](const RenderGrassData& grassData, CommandList& list) {

			PipelineStateSettings settings{};
			settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/VertGrass.slang");
			settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/Grass/FragGrass.slang");
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

			if (m_culledInstancesBuffer[frameIndex]->GetBuffer()) {
				list.GetList()->SetGraphicsRootShaderResourceView(3, m_culledInstancesBuffer[frameIndex]->GetBuffer()->GetGPUVirtualAddress());
			}

			list.GetList()->SetGraphicsRoot32BitConstants(
				0,
				sizeof(GrassRC) / 4,
				&m_rc,
				0
			);

			list.GetList()->IASetVertexBuffers(0, 1, &m_grassVertices->GetVBView()->View());
			list.GetList()->IASetIndexBuffer(&m_grassIndices->GetIBView()->View());

			// Indirect drawing to reduce cpu overhead and picking the correct LOD's. It executes a total of 3 draw commands for all LOD's
			list.GetList()->ExecuteIndirect(
				m_commandSignature.Get(),
				m_lodAmount,
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

		// Loop over all camera's TODO make the code run for each camera entity
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				FrustumData.viewProj = cam.GetView() * cam.GetProjection();
				FrustumData.cameraPos = cam.GetPosition();
			}
		}

		// Extracting the frustum data from the view and proj
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
		grassVertices.push_back({ {-0.06f, 0.2f, 0.0f}, 0.0f, 0.2f });
		grassVertices.push_back({ {0.06f, 0.2f, 0.0f}, 0.0f, 0.2f });
		grassVertices.push_back({ {-0.06f, 0.4f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {0.06f, 0.4f, 0.0f}, 0.0f, 0.4f });
		grassVertices.push_back({ {-0.06f, 0.6f, 0.0f}, 0.0f, 0.6f });
		grassVertices.push_back({ {0.06f, 0.6f, 0.0f}, 0.0f, 0.6f });
		grassVertices.push_back({ {0.0f, 0.75f, 0.0f}, 0.0f, 1.0f });

		grassIndices.insert(grassIndices.end(), { 0, 1, 2, 1, 3, 2 }); // First quad
		grassIndices.insert(grassIndices.end(), { 2, 3, 4, 3, 5, 4 }); // Second quad
		grassIndices.insert(grassIndices.end(), { 4, 5, 6, 5, 7, 6 }); // Third quad
		grassIndices.insert(grassIndices.end(), { 6, 7, 8 }); // Top triangle

		/// Second lod grass blade
		grassVertices.push_back({ {-0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.3f, 0.0f}, 0.0f, 0.3f });
		grassVertices.push_back({ {0.06f, 0.3f, 0.0f}, 0.0f, 0.3f });
		grassVertices.push_back({ {-0.06f, 0.6f, 0.0f}, 0.0f, 0.6f });
		grassVertices.push_back({ {0.06f, 0.6f, 0.0f}, 0.0f, 0.6f });
		grassVertices.push_back({ {0.0f, 0.75f, 0.0f}, 0.0f, 1.0f });

		grassIndices.insert(grassIndices.end(), { 0, 1, 2, 1, 3, 2 }); // First quad
		grassIndices.insert(grassIndices.end(), { 2, 3, 4, 3, 5, 4 }); // Second quad
		grassIndices.insert(grassIndices.end(), { 4, 5, 6 }); // Top triangle

		/// Third lod grass blade 
		grassVertices.push_back({ {-0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {0.06f, 0.0f, 0.0f}, 0.0f, 0.0f });
		grassVertices.push_back({ {-0.06f, 0.5f, 0.0f}, 0.0f, 0.5f });
		grassVertices.push_back({ {0.06f, 0.5f, 0.0f}, 0.0f, 0.5f });
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