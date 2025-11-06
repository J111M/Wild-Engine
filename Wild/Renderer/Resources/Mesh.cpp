#include "Renderer/Resources/Mesh.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
		if (vertices.size() == 0)
		{
			WD_WARN("No vertices supplied.");
			return;
		}

		if (indices.size() != 0)
			m_hasIndexBuffer = true;

		{
			BufferDesc desc{};
			m_vertexBuffer = std::make_shared<Buffer>(desc);
			m_vertexBuffer->create_vertex_buffer(vertices);

			m_drawCount = vertices.size();
		}

		if(m_hasIndexBuffer)
		{
			BufferDesc desc{};
			m_indexBuffer = std::make_shared<Buffer>(desc);
			m_vertexBuffer->CreateIndexBuffer(indices);

			m_drawCount = indices.size();
		}
	}
}