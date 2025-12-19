#pragma once

#include "Core/ECS.hpp"

#include <memory>

namespace Wild {
	struct GrassVertex {
		glm::vec3 Position{};
		float OneDCoordinates{};
		float Sway{};
	};

	struct SceneData {
		glm::mat4 ProjView{};
		glm::vec3 CameraPosition{};
	};

	struct GrassRC {
		glm::mat4 Matrix{};
		uint32_t bladeId{};
		float time;
		uint32_t foo2;
		uint32_t foo3;
	};

	class GrassManager
	{
	public:
		GrassManager();
		~GrassManager();

		void Update();
		void Render(CommandList& list, std::shared_ptr<Buffer> GrassData, float deltaTime);
	private:
		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_grassBuffer;
		std::shared_ptr<Buffer> m_sceneData[BACK_BUFFER_COUNT];

		float m_accumulatedTime{};

		GrassRC m_rc{};
		Entity m_chunkEntity;

		PipelineStateSettings m_settings;
		std::shared_ptr<PipelineState> m_pipeline{};
	};

}