#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct DeferredPassData
	{
		Texture* FinalTexture;
	};

	class DeferredPass : public RenderFeature
	{
	public:
		DeferredPass();
		~DeferredPass() {};

		virtual void Add(Renderer& renderer, RenderGraph& rg) override;

	private:

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		RootConstant m_rc;

		PipelineStateSettings m_settings;
		std::shared_ptr<PipelineState> m_pipeline{};
	};
}