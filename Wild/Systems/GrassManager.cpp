#include "Systems/GrassManager.hpp"

#include <vector>

namespace Wild {
	GrassManager::GrassManager()
	{
		auto device = engine.GetGfxContext();

		m_vertShader = std::make_shared<Shader>("Shaders/vertGrassShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragGrassShader.hlsl");

		m_settings.ShaderState.VertexShader = m_vertShader;
		m_settings.ShaderState.FragShader = m_fragShader;
		m_settings.DepthStencilState.DepthEnable = true;

		// Setting up the input layout
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("COORDS", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3)));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("SWAY", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3) + sizeof(float)));

		m_settings.RasterizerState.CullMode = CullMode::None;

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

		m_pipeline = std::make_shared<PipelineState>(PipelineStateType::Graphics, m_settings, uniforms);

		std::vector<GrassVertex> grassBlade{};

		// Grass blade vertice data | position, 1D coordinates and sway
		grassBlade.push_back({ {-0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.06,1.0,0.0}, 0.0, 0.4 });

		grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.06,1.0,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {-0.06,1.0,0.0}, 0.0, 0.4 });

		grassBlade.push_back({ {-0.06,1.0,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {0.06,1.0,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {-0.06,1.5,0.0}, 0.0, 0.7 });

		grassBlade.push_back({ {0.06,1.0,0.0}, 0.0, 0.4 });
		grassBlade.push_back({ {0.06,1.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {-0.06,1.5,0.0}, 0.0, 0.7 });

		grassBlade.push_back({ {-0.06,1.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {0.06,1.5,0.0}, 0.0, 0.7 });
		grassBlade.push_back({ {0.0,2.3,0.0}, 0.0, 1.0 });

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

	void GrassManager::Update() {
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
	}

	void GrassManager::Render(CommandList& list, std::shared_ptr<Buffer> GrassData) {
		auto& gfxContext = engine.GetGfxContext();

		auto& transform = engine.GetECS()->GetComponent<Transform>(m_chunkEntity);

		m_rc.Matrix = transform.GetWorldMatrix();

		list.GetList()->SetPipelineState(m_pipeline->GetPso().Get());

		list.GetList()->SetGraphicsRootSignature(m_pipeline->GetRootSignature().Get());

		list.GetList()->SetGraphicsRootShaderResourceView(
			1,
			GrassData->GetBuffer()->GetGPUVirtualAddress()
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

		/*for (size_t i = 0; i < ; i++)
		{
			
		}*/
		
	}
}