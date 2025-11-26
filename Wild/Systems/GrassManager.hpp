#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/PipelineStateBuilder.hpp"

#include <memory>

namespace Wild {
	struct GrassVertex {
		glm::vec3 Position{};
		float OneDCoordinates{};
		float Sway{};
	};

	class GrassManager
	{
	public:
		GrassManager();
		~GrassManager();

		void Update() {};
		void Render(CommandList& list);
	private:
		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_grassBuffer;

		std::shared_ptr<PipelineState> m_pipeline;

		PipelineStateSettings m_settings;
	};

}