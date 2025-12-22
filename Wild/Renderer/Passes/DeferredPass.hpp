#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct DeferredPassData
	{
		Texture* FinalTexture;
		Texture* DepthTexture;
	};

	class DeferredPass : public RenderFeature
	{
	public:
		DeferredPass();
		~DeferredPass() {};

		virtual void Add(Renderer& renderer, RenderGraph& rg) override;
		virtual void Update(const float dt) override;
	private:

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		RootConstant m_rc;
		std::shared_ptr<PipelineState> m_pipeline{};


		std::unique_ptr<Texture> m_texture;
	};
}