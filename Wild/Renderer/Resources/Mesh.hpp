#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Core/Transform.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace Wild {
	struct Vertex {
		glm::vec3 position{};
		glm::vec3 color{};
		glm::vec3 normal{};
		glm::vec2 uv{};
	};

	class Mesh
	{
	public:
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices = {});
		~Mesh() {};

		std::shared_ptr<Buffer> GetVertexBuffer() const { m_vertexBuffer; }
		std::shared_ptr<Buffer> GetIndexBuffer() const { m_indexBuffer; }

		uint32_t GetDrawCount() const { return m_drawCount; }

	private:
		std::shared_ptr<Buffer> m_vertexBuffer;
		std::shared_ptr<Buffer> m_indexBuffer;

		bool m_hasIndexBuffer = false;
		uint32_t m_drawCount{};
	};
}