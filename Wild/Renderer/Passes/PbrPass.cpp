#include "PbrPass.hpp"
#include "PbrPass.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

namespace Wild {
	PbrPass::PbrPass()
	{
		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(InverseCamera);
			for (int i = 0; i < BACK_BUFFER_COUNT; i++)
			{
				m_inverseCamera[i] = std::make_unique<Buffer>(desc, BufferType::constant);
			}
		}

		{
			BufferDesc desc{};
			desc.bufferSize = sizeof(PBRData);
			for (int i = 0; i < BACK_BUFFER_COUNT; i++)
			{
				m_pbrData[i] = std::make_unique<Buffer>(desc, BufferType::constant);
			}
		}

	}

	void PbrPass::Update(const float dt)
	{
		auto context = engine.GetGfxContext();
		auto ecs = engine.GetECS();
		auto& cameras = ecs->View<Camera>();

		PBRData pbrData{};
		InverseCamera inverseCamData{};

		// Loop over all camera's TODO make the code run for each camera entity
		for (auto& cameraEntity : cameras) {
			if (ecs->HasComponent<Camera>(cameraEntity)) {
				auto& cam = ecs->GetComponent<Camera>(cameraEntity);

				inverseCamData.inverseView = glm::inverse(cam.GetView());
				inverseCamData.inverseProj = glm::inverse(cam.GetProjection());
				pbrData.cameraPosition = cam.GetPosition();
			}
		}

		int frameIndex = context->GetBackBufferIndex();
		m_inverseCamera[frameIndex]->Allocate(&inverseCamData);

		pbrData.lightDirection = glm::vec3(-0.3, -6.0, -2.5);
		m_pbrData[frameIndex]->Allocate(&pbrData);
	}

	void PbrPass::Add(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<PbrPassData>();
		auto* deferredData = rg.GetPassData<PbrPassData, DeferredPassData>();

		passData->DepthTexture = deferredData->DepthTexture;

		// Final texture
		{
			TextureDesc desc;
			desc.width = engine.GetGfxContext()->GetWidth();
			desc.Height = engine.GetGfxContext()->GetHeight();
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.name = "FinalPbrTexture";
			desc.usage = TextureDesc::gpuOnly;
			desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
			passData->FinalTexture = rg.CreateTransientTexture("FinalPbrTexture", desc);
		}

		rg.AddPass<PbrPassData>(
			"Pbr assembly pass",
			PassType::Graphics,
			[&renderer, deferredData, this](const PbrPassData& passData, CommandList& list) {
			PipelineStateSettings settings{};
			settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/PbrVert.slang");
			settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/PbrFrag.slang");
			settings.DepthStencilState.DepthEnable = false;
			settings.RasterizerState.WindingMode = WindingOrder::Clockwise;

			std::vector<Uniform> uniforms;

			{
				Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(PbrRootConstant) };
				uni.Visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				uniforms.emplace_back(uni);
			}

			// Inverse camera buffer
			{
				Uniform uni{ 1, 0, RootParams::RootResourceType::ConstantBufferView };
				uni.Visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				uniforms.emplace_back(uni);
			}

			{
				Uniform uni{ 2, 0, RootParams::RootResourceType::ConstantBufferView };
				uni.Visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				uniforms.emplace_back(uni);
			}

			// Bindless resources
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

			auto& pipeline = renderer.GetOrCreatePipeline("Pbr assembly pass", PipelineStateType::Graphics, settings, uniforms);

			list.SetPipelineState(pipeline);
			list.BeginRender({ passData.FinalTexture }, { ClearOperation::Store }, nullptr, DSClearOperation::Store, "Pbr assembly pass");

			deferredData->AlbedoRoughnessTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			deferredData->NormalMetallicTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			deferredData->EmissiveTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			deferredData->DepthTexture->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			m_rc.albedoView = deferredData->AlbedoRoughnessTexture->GetSrv()->BindlessView();
			m_rc.normalView = deferredData->NormalMetallicTexture->GetSrv()->BindlessView();
			m_rc.emissiveView = deferredData->EmissiveTexture->GetSrv()->BindlessView();
			m_rc.depthView = deferredData->DepthTexture->GetSrv()->BindlessView();

			list.SetRootConstant<PbrRootConstant>(0, m_rc);

			auto context = engine.GetGfxContext();
			int frameIndex = context->GetBackBufferIndex();

			list.SetConstantBufferView(1, m_inverseCamera[frameIndex].get());
			list.SetConstantBufferView(2, m_pbrData[frameIndex].get());
			list.SetBindlessHeap(3);

			list.GetList()->DrawInstanced(3, 1, 0, 0);
			list.EndRender();
		});
	}
}