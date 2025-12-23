#include "Systems/GrassManager.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

#include <vector>

namespace Wild {
	GrassManager::GrassManager(std::shared_ptr<Buffer> GrassData)
	{
		m_grassDataBuffer = GrassData;
		std::vector<GrassVertex> grassBlade{};

		// Grass blade vertice data | position, 1D coordinates and sway
		grassBlade.push_back({ {-0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });

		grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });

		grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });

		grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {0.06,0.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });

		grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {0.06,0.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {0.0,0.7,0.0}, 0.0, 1.0 });

		{
			BufferDesc desc{};
			m_grassBuffer = std::make_shared<Buffer>(desc);
			m_grassBuffer->CreateVertexBuffer<GrassVertex>(grassBlade);
		}

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

	GrassManager::~GrassManager()
	{

	}

	void GrassManager::Update(const float dt)
	{
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

	void GrassManager::Add(Renderer& renderer, RenderGraph& rg)
	{
		auto* grassPassData = rg.AllocatePassData<GrassPassData>();
		auto* deferredData = rg.GetPassData<GrassPassData, DeferredPassData>();

		grassPassData->FinalTexture = deferredData->FinalTexture;
		grassPassData->DepthTexture = deferredData->DepthTexture;

		rg.AddPass<GrassPassData>(
			"Grass Pass",
			PassType::Graphics,
			[&renderer, this](const GrassPassData& passData, CommandList& list)
			{
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

				auto& pipeline = renderer.GetOrCreatePipeline("Grass Pass", PipelineStateType::Graphics, settings, uniforms);

				list.SetPipelineState(pipeline);
				list.BeginRender({ passData.FinalTexture },
					{ ClearOperation::Store },
					passData.DepthTexture,
					DSClearOperation::Store,
					"Grass Pass");

				auto& gfxContext = engine.GetGfxContext();

				auto& transform = engine.GetECS()->GetComponent<Transform>(m_chunkEntity);
				m_rc.Matrix = transform.GetWorldMatrix();

				list.GetList()->SetGraphicsRootShaderResourceView(
					1,
					m_grassDataBuffer->GetBuffer()->GetGPUVirtualAddress()
				);

				list.GetList()->SetGraphicsRootConstantBufferView(
					2,
					m_sceneData[gfxContext->GetBackBufferIndex()]->GetBuffer()->GetGPUVirtualAddress()
				);

				m_rc.bladeId = 0;


				list.GetList()->SetGraphicsRoot32BitConstants(
					0,
					sizeof(GrassRC) / 4,
					&m_rc,
					0
				);

				list.GetList()->IASetVertexBuffers(0, 1, &m_grassBuffer->GetVBView()->View());
				list.GetList()->DrawInstanced(15, MAXGRASSBLADES, 0, 0);

				renderer.CompositeTexture = passData.FinalTexture;

				list.EndRender();
			});
	}
	
}