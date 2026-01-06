#include "Renderer/Passes/DeferredPass.hpp"

#include "Renderer/Resources/Model.hpp"

namespace Wild {
	DeferredPass::DeferredPass()
	{
		auto ecs = engine.GetECS();

		auto entity = ecs->CreateEntity();
		auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0, 0, 0), entity);
		ecs->AddComponent<Model>(entity, "Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", entity);
		transform.SetPosition(glm::vec3(0, 0, -10));

		m_texture = std::make_unique<Texture>("Assets/Models/DamagedHelmet/glTF/Default_albedo.jpg");
	}

	void DeferredPass::Update(const float dt)
	{
	}

	void DeferredPass::Add(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<DeferredPassData>();

		// Final Texture
		{
			TextureDesc desc;
			desc.width = engine.GetGfxContext()->GetWidth();
			desc.Height = engine.GetGfxContext()->GetHeight();
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.name = "Final Texture";
			desc.usage = TextureDesc::gpuOnly;
			desc.flag = static_cast<TextureDesc::ViewFlag>(TextureDesc::renderTarget | TextureDesc::shaderResource);
			passData->FinalTexture = rg.CreateTransientTexture("FinalTexture", desc);
		}

		// DepthStencil texture.
		{
			TextureDesc desc;
			desc.width = engine.GetGfxContext()->GetWidth();
			desc.Height = engine.GetGfxContext()->GetHeight();
			desc.name = "DepthStencil Texture";
			desc.usage = TextureDesc::gpuOnly;
			desc.flag = TextureDesc::depthStencil; // Automatically uses depth stencil format
			passData->DepthTexture = rg.CreateTransientTexture("DepthStencil", desc);
		}

		rg.AddPass<DeferredPassData>(
			"Deferred pass",
			PassType::Graphics,
			[&renderer, this](const DeferredPassData& passData, CommandList& list)
			{
				PipelineStateSettings settings{};
				settings.ShaderState.VertexShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/VertPbr.slang");
				settings.ShaderState.FragShader = engine.GetShaderTracker()->GetOrCreateShader("Shaders/FragPbr.slang");
				settings.DepthStencilState.DepthEnable = true;

				// Setting up the input layout
				settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
				settings.ShaderState.InputLayout.emplace_back(InputElement("COLOR", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3)));
				settings.ShaderState.InputLayout.emplace_back(InputElement("NORMAL", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(glm::vec3) * 2));
				settings.ShaderState.InputLayout.emplace_back(InputElement("TEXCOORD", DXGI_FORMAT_R32G32_FLOAT, sizeof(glm::vec3) * 3));

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

				auto& pipeline = renderer.GetOrCreatePipeline("Deferred pass", PipelineStateType::Graphics, settings, uniforms);

				// Rendering
				auto ecs = engine.GetECS();
				auto& cameras = ecs->View<Camera>();

				Camera* camera = nullptr;
				for (auto entity : cameras) {
					camera = &ecs->GetComponent<Camera>(entity);
					break;
				}

				list.SetPipelineState(pipeline);
				list.BeginRender({ passData.FinalTexture }, { ClearOperation::Clear }, { passData.DepthTexture }, DSClearOperation::DepthClear);

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

					list.GetList()->SetGraphicsRootDescriptorTable(1, engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());

					list.GetList()->IASetVertexBuffers(0, 1, &mesh.GetVertexBuffer()->GetVBView()->View());

					if (mesh.HasIndexBuffer()) {
						list.GetList()->IASetIndexBuffer(&mesh.GetIndexBuffer()->GetIBView()->View());
						list.GetList()->DrawIndexedInstanced(mesh.GetDrawCount(), 1, 0, 0, 0);
					}
					else {
						list.GetList()->DrawInstanced(mesh.GetDrawCount(), 1, 0, 0);
					}
				}

				list.EndRender();

			});
	}
}