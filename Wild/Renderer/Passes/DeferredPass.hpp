#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct RootConstant {
		glm::mat4 matrix{};
		glm::mat4 invMatrix{};
		uint32_t albedoView{};
		uint32_t normalView{};
		uint32_t roughnessMetallicView{};
		uint32_t emissiveView{};
	};

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