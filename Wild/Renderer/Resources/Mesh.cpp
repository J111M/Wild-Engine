#include "Renderer/Resources/Mesh.hpp"

#include "Renderer/Resources/Buffer.hpp"

#include "Tools/Log.hpp"

namespace Wild
{
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        if (vertices.size() == 0)
        {
            WD_WARN("No vertices supplied.");
            return;
        }

        // If mesh shaders are supported create the data for the meshlets
        if (engine.GetGfxContext()->GetCapabilities().SupportsMeshShaders())
        {
            m_meshlets = std::make_unique<Meshlets>(vertices, indices);
        }

        {
            BufferDesc desc{};
            desc.usage = BufferUsage::Vertex | BufferUsage::ShaderRead;
            desc.access = MemoryAccess::GpuOnly;

            m_vertexBuffer = std::make_shared<GPUBuffer>(desc);
            m_vertexBuffer->CreateVertexBuffer<Vertex>(vertices);

            m_vertexCount = vertices.size();
            m_drawCount = vertices.size();
        }

        if (indices.size() != 0) m_hasIndexBuffer = true;

        if (m_hasIndexBuffer)
        {
            BufferDesc desc{};
            desc.usage = BufferUsage::Index;
            desc.access = MemoryAccess::GpuOnly;

            m_indexBuffer = std::make_shared<GPUBuffer>(desc);
            m_indexBuffer->CreateIndexBuffer(indices);

            m_drawCount = indices.size();
        }

        m_collisionPositions.reserve(vertices.size());
        for (const auto& vertex : vertices)
            m_collisionPositions.push_back(vertex.position);

        m_collisionIndices = indices;
    }
} // namespace Wild
