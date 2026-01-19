#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct DeferredPassData
	{
		Texture* AlbedoRoughnessTexture;
		Texture* NormalMetallicTexture;
		Texture* EmissiveTexture;
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
		RootConstant m_rc;
		std::shared_ptr<PipelineState> m_pipeline{};


		std::unique_ptr<Texture> m_texture;
	};
}