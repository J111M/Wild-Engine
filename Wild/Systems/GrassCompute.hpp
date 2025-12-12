#pragma once

#include "Renderer/RenderPass.hpp"

namespace Wild{

#define MAXGRASSBLADES 5000

	struct GrassBladeData {
		glm::vec3 position{};
		float rotation{};
		float height{};
	};

	struct GrassComputeRC {
		glm::vec2 minMaxHeight{};
		uint32_t seed{};
	};

	class GrassCompute : RenderPass
	{
	public:
		GrassCompute();
		~GrassCompute() {};

		void Update() {};
		void Render(CommandList& list);

		std::shared_ptr<Buffer> GetGrassData() { return m_bladeDataBuffer; }

	private:
		GrassComputeRC m_rc{};

		std::shared_ptr<Buffer> m_bladeDataBuffer;
		std::shared_ptr<Shader> m_computeShader;
	};
}