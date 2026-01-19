#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct PbrPassData
	{
		Texture* FinalTexture;
		Texture* DepthTexture;
	};

	struct InverseCamera {
		glm::mat4 inverseView{};
		glm::mat4 inverseProj{};
	};

	struct PBRData {
		glm::vec3 cameraPosition{};
		float pad{};
		glm::vec3 lightDirection{};
		float pad1{};
	};

	struct PbrRootConstant {
		uint32_t emissiveView{};
		uint32_t albedoView{};
		uint32_t normalView{};
		uint32_t depthView{};
	};

	class PbrPass : public RenderFeature
	{
	public:
		PbrPass();
		~PbrPass() {};

		virtual void Add(Renderer& renderer, RenderGraph& rg) override;
		virtual void Update(const float dt) override;

	private:
		PbrRootConstant m_rc{};
		std::unique_ptr<Buffer> m_inverseCamera[BACK_BUFFER_COUNT];
		std::unique_ptr<Buffer> m_pbrData[BACK_BUFFER_COUNT];
	};	
}