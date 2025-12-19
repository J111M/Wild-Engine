#include "Renderer/Passes/DeferredPass.hpp"

namespace Wild {
	DeferredPass::DeferredPass()
	{
		m_vertShader = std::make_shared<Shader>("Shaders/vertShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragShader.hlsl");

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
	}

	void DeferredPass::Add(Renderer& renderer, RenderGraph& rg)
	{
		auto* passData = rg.AllocatePassData<DeferredPassData>();

		rg.AddPass<DeferredPassData>(
			"Deferred pass",
			PassType::Graphics,
			[&renderer, this](const DeferredPassData& passData, CommandList& list)
			{
				
			});
	}
}