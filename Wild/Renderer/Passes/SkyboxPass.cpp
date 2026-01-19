#include "Renderer/Passes/SkyboxPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"

namespace Wild {
	SkyPass::SkyPass(std::string filePath) {
		m_cube = CreateCube();
		BufferDesc desc;
		m_cubeVertexBuffer = std::make_unique<Buffer>(desc);
		m_cubeVertexBuffer->CreateVertexBuffer<Vertex>(m_cube);

		BufferDesc bufferDesc{};
		bufferDesc.bufferSize = sizeof(CameraProjection);
		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			m_cameraProjection[i] = std::make_unique<Buffer>(bufferDesc, BufferType::constant);
		}

		m_skyboxTexture = std::make_unique<Texture>(filePath, TextureType::SKYBOX, 0, DXGI_FORMAT_R32G32B32A32_FLOAT);
	}

	void SkyPass::Update(const float dt)
	{
		auto context = engine.GetGfxContext();
		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		CameraProjection camData{};

		// Loop over all camera's TODO make the code run for each camera entity
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				camData.view = glm::mat4(glm::mat3(cam.GetView()));
				camData.proj = cam.GetProjection();
				//camData.proj[1][1] *= -1;
			}
		}

		int frameIndex = context->GetBackBufferIndex();
		m_cameraProjection[frameIndex]->Allocate(&camData);
	}

	void SkyPass::Add(Renderer& renderer, RenderGraph& rg)
	{
		AddSkyboxPass(renderer, rg);
	}

	void SkyPass::AddSkyboxPass(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<SkyPassData>();
		auto* pbrData = rg.GetPassData<SkyPassData, PbrPassData>();

		passData->FinalTexture = pbrData->FinalTexture;
		passData->DepthTexture = pbrData->DepthTexture;

		rg.AddPass<SkyPassData>(
			"Skybox pass",
			PassType::Graphics,
			[&renderer, this](const SkyPassData& passData, CommandList& list) {
			PipelineStateSettings settings{};
			settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/SkyboxVert.slang");
			settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/SkyboxFrag.slang");
			settings.DepthStencilState.DepthEnable = true;
			settings.DepthStencilState.DepthWriteMask = DepthWriteMask::Zero;
			settings.DepthStencilState.DepthFunc = ComparisonFunc::LessEqual;
			settings.RasterizerState.CullMode = CullMode::None;
			//settings.RasterizerState.WindingMode = WindingOrder::Clockwise;

			// Setting up the input layout
			settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
			settings.ShaderState.InputLayout.emplace_back(InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
			settings.ShaderState.InputLayout.emplace_back(InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
			settings.ShaderState.InputLayout.emplace_back(InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

			std::vector<Uniform> uniforms;

			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(SkyRootConstant) };
				uniforms.emplace_back(uni);
			}

			// Camera buffer
			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::ConstantBufferView };
				uniforms.emplace_back(uni);
			}

			// Bindless heap
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
				uni.Visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				uniforms.emplace_back(uni);
			}

			auto& pipeline = renderer.GetOrCreatePipeline("Skybox pass", PipelineStateType::Graphics, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender({ passData.FinalTexture }, { ClearOperation::Store }, passData.DepthTexture, DSClearOperation::Store, "Skybox pass");

			auto context = engine.GetGfxContext();
			int frameIndex = context->GetBackBufferIndex();

			SkyRootConstant rc{};
			rc.view = m_skyboxTexture->GetSrv()->BindlessView();

			list.GetList()->SetGraphicsRoot32BitConstants(0, sizeof(SkyRootConstant) / 4, &rc, 0);

			list.GetList()->SetGraphicsRootConstantBufferView(1, m_cameraProjection[frameIndex]->GetBuffer()->GetGPUVirtualAddress());

			m_skyboxTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			list.GetList()->SetGraphicsRootDescriptorTable(2, context->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());

			list.GetList()->IASetVertexBuffers(0, 1, &m_cubeVertexBuffer->GetVBView()->View());

			list.GetList()->DrawInstanced(36, 1, 0, 0);
			list.EndRender();
		});
	}

	std::vector<Vertex> Wild::SkyPass::CreateCube()
	{
		return {
			// Front face
			{{-0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0, 1}, {0, 0}},
			{{ 0.5f, -0.5f,  0.5f}, {0, 1, 0}, {0, 0, 1}, {1, 0}},
			{{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 0, 1}, {1, 1}},
			{{-0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0, 1}, {0, 0}},
			{{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 0, 1}, {1, 1}},
			{{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0, 1}, {0, 1}},

			// Back face
			{{ 0.5f, -0.5f, -0.5f}, {1, 1, 0}, {0, 0, -1}, {0, 0}},
			{{-0.5f, -0.5f, -0.5f}, {0, 1, 1}, {0, 0, -1}, {1, 0}},
			{{-0.5f,  0.5f, -0.5f}, {1, 0, 1}, {0, 0, -1}, {1, 1}},
			{{ 0.5f, -0.5f, -0.5f}, {1, 1, 0}, {0, 0, -1}, {0, 0}},
			{{-0.5f,  0.5f, -0.5f}, {1, 0, 1}, {0, 0, -1}, {1, 1}},
			{{ 0.5f,  0.5f, -0.5f}, {0, 1, 1}, {0, 0, -1}, {0, 1}},

			// Left face
			{{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {-1, 0, 0}, {0, 0}},
			{{-0.5f, -0.5f,  0.5f}, {0, 1, 0}, {-1, 0, 0}, {1, 0}},
			{{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {-1, 0, 0}, {1, 1}},
			{{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {-1, 0, 0}, {0, 0}},
			{{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {-1, 0, 0}, {1, 1}},
			{{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {-1, 0, 0}, {0, 1}},

			// Right face
			{{ 0.5f, -0.5f,  0.5f}, {1, 1, 0}, {1, 0, 0}, {0, 0}},
			{{ 0.5f, -0.5f, -0.5f}, {1, 0, 1}, {1, 0, 0}, {1, 0}},
			{{ 0.5f,  0.5f, -0.5f}, {0, 1, 1}, {1, 0, 0}, {1, 1}},
			{{ 0.5f, -0.5f,  0.5f}, {1, 1, 0}, {1, 0, 0}, {0, 0}},
			{{ 0.5f,  0.5f, -0.5f}, {0, 1, 1}, {1, 0, 0}, {1, 1}},
			{{ 0.5f,  0.5f,  0.5f}, {1, 0, 1}, {1, 0, 0}, {0, 1}},

			// Top face
			{{-0.5f,  0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 0}},
			{{ 0.5f,  0.5f, -0.5f}, {0, 1, 1}, {0, 1, 0}, {1, 0}},
			{{ 0.5f,  0.5f,  0.5f}, {1, 0, 1}, {0, 1, 0}, {1, 1}},
			{{-0.5f,  0.5f, -0.5f}, {1, 1, 1}, {0, 1, 0}, {0, 0}},
			{{ 0.5f,  0.5f,  0.5f}, {1, 0, 1}, {0, 1, 0}, {1, 1}},
			{{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 1, 0}, {0, 1}},

			// Bottom face
			{{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, -1, 0}, {0, 0}},
			{{ 0.5f, -0.5f, -0.5f}, {0, 0, 1}, {0, -1, 0}, {1, 0}},
			{{ 0.5f, -0.5f,  0.5f}, {0, 1, 0}, {0, -1, 0}, {1, 1}},
			{{-0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, -1, 0}, {0, 0}},
			{{ 0.5f, -0.5f,  0.5f}, {0, 1, 0}, {0, -1, 0}, {1, 1}},
			{{-0.5f, -0.5f,  0.5f}, {1, 1, 0}, {0, -1, 0}, {0, 1}}
		};
	}
}