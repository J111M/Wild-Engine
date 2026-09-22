#include "Renderer/Resources/Meshlets.hpp"

namespace Wild
{
    Meshlets::Meshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& meshName)
    {
        const size_t maxVertices = 64;
        const size_t maxTriangles = 124;
        const size_t indicesSize = indices.size();

        size_t maxMeshlets = meshopt_buildMeshletsBound(indicesSize, maxVertices, maxTriangles);

        std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
        std::vector<unsigned int> meshletVertices(indicesSize);
        std::vector<unsigned char> meshletTriangles(indicesSize);

        std::vector<uint32_t> primitiveVertexPositions;

        // Build the meshlets out of the current mesh
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

        std::vector<Meshlet> meshletCollection;
        meshletCollection.reserve(meshlets.size());

        for (meshopt_Meshlet& meshlet : meshlets)
        {
            meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertex_offset],
                                                                 &meshletTriangles[meshlet.triangle_offset],
                                                                 meshlet.triangle_count,
                                                                 reinterpret_cast<const float*>(vertices.data()),
                                                                 vertices.size(),
                                                                 sizeof(Vertex));

            Meshlet& m = meshletCollection.emplace_back();
            m.meshletBoundingSphere = glm::vec4(bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
            m.coneApex = glm::make_vec3(bounds.cone_apex);
            m.coneCutoff = bounds.cone_cutoff;
            m.coneAxis = glm::make_vec3(bounds.cone_axis);
            m.vertexOffset = 0;
            m.meshletVerticesOffset = meshlet.vertex_offset;
            m.meshletTriangleOffset = meshlet.triangle_offset;
            m.meshletVerticesCount = meshlet.vertex_count;
            m.meshletTriangleCount = meshlet.triangle_count;
        }

        {
            BufferDesc desc{};
            desc.name = "Meshlet buffer: " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(meshletCollection.size());
            desc.stride = sizeof(Meshlet);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.meshletBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.meshletBuffer->Allocate(meshletCollection.data());
        }

        {
            BufferDesc desc{};
            desc.name = "Vertices buffer of meshlet: " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(meshletVertices.size());
            desc.stride = sizeof(uint32_t);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.uniqueVertexIndexBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.uniqueVertexIndexBuffer->Allocate(meshletVertices.data());
        }

        std::vector<uint32_t> primitiveIndices(meshletTriangles.begin(), meshletTriangles.end());

        {
            BufferDesc desc{};
            desc.name = "Primitive index buffer of meshlet " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(primitiveIndices.size());
            desc.stride = sizeof(Meshlet);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.primitiveIndexBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.primitiveIndexBuffer->Allocate(primitiveIndices.data());
        }

        m_meshletData.meshletCount = static_cast<uint32_t>(meshletCollection.size());
    }
} // namespace Wild
