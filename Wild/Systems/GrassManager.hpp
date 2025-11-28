#pragma once

#include "Renderer/RenderPass.hpp"

#include "Core/ECS.hpp"

#include <memory>

namespace Wild {
	struct GrassVertex {
		glm::vec3 Position{};
		float OneDCoordinates{};
		float Sway{};
	};

	struct GrassRC {
		glm::mat4 matrix{};
		uint32_t bladeId{};
		uint32_t foo1;
		uint32_t foo2;
		uint32_t foo3;
	};

	class GrassManager : RenderPass
	{
	public:
		GrassManager();
		~GrassManager();

		void Update() {};
		void Render(CommandList& list, std::shared_ptr<Buffer> GrassData);
	private:
		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_grassBuffer;

		GrassRC m_rc{};
		Entity m_chunkEntity;
	};

}