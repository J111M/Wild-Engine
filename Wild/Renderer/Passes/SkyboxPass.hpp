#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Resources/Mesh.hpp"

namespace Wild {
	struct SkyPassData
	{
		Texture* FinalTexture;
		Texture* DepthTexture;
	};

	struct CameraProjection {
		glm::mat4 view{};
		glm::mat4 proj{};
	};

	struct SkyRootConstant {
		uint32_t view;
	};

	class SkyPass : public RenderFeature
	{
	public:
		SkyPass(std::string filePath);
		~SkyPass() {};

		virtual void Update(const float dt) override;
		virtual void Add(Renderer& renderer, RenderGraph& rg) override;

		void AddSkyboxPass(Renderer& renderer, RenderGraph& rg);

	private:
		std::vector<Vertex> CreateCube();
		std::vector<Vertex> m_cube;

		std::unique_ptr<Buffer> m_cameraProjection[BACK_BUFFER_COUNT];
		std::unique_ptr<Buffer> m_cubeVertexBuffer;
		std::unique_ptr<Texture> m_skyboxTexture;
	};
}