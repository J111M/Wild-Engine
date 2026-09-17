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

        if (engine.GetGfxContext()->GetCapabilities().SupportsMeshShaders())
        {
            const size_t maxVertices = 64;
            const size_t maxTriangles = 124;
            const size_t indicesSize = indices.size();

            size_t maxMeshlets = meshopt_buildMeshletsBound(indicesSize, maxVertices, maxTriangles);

            std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
            std::vector<unsigned int> meshletVertices(indicesSize);
            std::vector<unsigned char> meshletTriangles(indicesSize);

            std::vector<uint32_t> primitiveVertexPositions;

            const size_t meshletCount = meshopt_buildMeshlets(&meshlets[0],
                                                              &meshletVertices[0],
                                                              &meshletTriangles[0],
                                                              indices.data(),
                                                              indicesSize,
                                                              reinterpret_cast<const float*>(vertices.data()),
                                                              vertices.size(),
                                                              sizeof(Vertex),
                                                              maxVertices,
                                                              maxTriangles,
                                                              0.25f);

            meshlets.resize(meshletCount);

            // Trim the memory that is not written too
            const meshopt_Meshlet& last = meshlets.back();
            meshletVertices.resize(last.vertex_offset + last.vertex_count);
            meshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

            // Reorder vertices and triangles within the meshlets for better GPU vertex cache utilization
            for (auto& meshlet : meshlets)
            {
                meshopt_optimizeMeshlet(&meshletVertices[meshlet.vertex_offset],
                                        &meshletTriangles[meshlet.triangle_offset],
                                        meshlet.triangle_count,
                                        meshlet.vertex_count);
            }
        }

        {
            BufferDesc desc{};
            desc.usage = BufferUsage::Vertex;
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
