#include "Renderer/Resources/Meshlets.hpp"

#include <cstddef>
#include <numeric>

namespace Wild
{
    Meshlets::Meshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& meshName)
    {
        const size_t maxVertices = 64;
        const size_t maxTriangles = 124;

        if (vertices.empty()) return;

        std::vector<uint32_t> generatedIndices;
        if (indices.empty())
        {
            generatedIndices.resize(vertices.size());
            std::iota(generatedIndices.begin(), generatedIndices.end(), 0u);
        }
        const auto& sourceIndices = indices.empty() ? generatedIndices : indices;
        const size_t indicesSize = sourceIndices.size() / 3 * 3;
        if (indicesSize == 0) return;

        if (std::any_of(sourceIndices.begin(), sourceIndices.begin() + indicesSize, [&vertices](uint32_t index) {
                return index >= vertices.size();
            }))
        {
            WD_WARN("Invalid vertex index supplied for meshlets: {}", meshName);
            return;
        }

        size_t maxMeshlets = meshopt_buildMeshletsBound(indicesSize, maxVertices, maxTriangles);

        std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
        std::vector<unsigned int> meshletVertices(indicesSize);
        std::vector<unsigned char> meshletTriangles(indicesSize);

        // Build the meshlets out of the current mesh
        const size_t meshletCount = meshopt_buildMeshlets(&meshlets[0],
                                                          &meshletVertices[0],
                                                          &meshletTriangles[0],
                                                          sourceIndices.data(),
                                                          indicesSize,
                                                          reinterpret_cast<const float*>(vertices.data()),
                                                          vertices.size(),
                                                          sizeof(Vertex),
                                                          maxVertices,
                                                          maxTriangles,
                                                          0.25f);

        meshlets.resize(meshletCount);
        if (meshlets.empty()) return;

        // Trim the memory that is not written too
        const meshopt_Meshlet& last = meshlets.back();
        meshletVertices.resize(last.vertex_offset + last.vertex_count);
        meshletTriangles.resize(last.triangle_offset + last.triangle_count * 3);

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
            desc.size = static_cast<uint64_t>(meshletCollection.size()) * sizeof(Meshlet);
            desc.stride = sizeof(Meshlet);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.meshletBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.meshletBuffer->UploadToGPU(meshletCollection.data());
        }

        {
            BufferDesc desc{};
            desc.name = "Vertices buffer of meshlet: " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(meshletVertices.size()) * sizeof(uint32_t);
            desc.stride = sizeof(uint32_t);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.uniqueVertexIndexBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.uniqueVertexIndexBuffer->UploadToGPU(meshletVertices.data());
        }

        std::vector<uint32_t> primitiveIndices(meshletTriangles.begin(), meshletTriangles.end());

        {
            BufferDesc desc{};
            desc.name = "Primitive index buffer of meshlet " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(primitiveIndices.size()) * sizeof(uint32_t);
            desc.stride = sizeof(uint32_t);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.primitiveIndexBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.primitiveIndexBuffer->UploadToGPU(primitiveIndices.data());
        }

        // Store meshlet id which is used for debugging
        std::vector<uint32_t> vertexMeshletIds(vertices.size(), 0);
        for (size_t meshletIndex = 0; meshletIndex < meshlets.size(); meshletIndex++)
        {
            const meshopt_Meshlet& meshlet = meshlets[meshletIndex];
            for (uint32_t i = 0; i < meshlet.vertex_count; i++)
                vertexMeshletIds[meshletVertices[meshlet.vertex_offset + i]] = static_cast<uint32_t>(meshletIndex);
        }

        {
            BufferDesc desc{};
            desc.name = "Vertex meshlet id buffer of meshlet: " + meshName;
            desc.usage = BufferUsage::ShaderRead;
            desc.size = static_cast<uint64_t>(vertexMeshletIds.size()) * sizeof(uint32_t);
            desc.stride = sizeof(uint32_t);
            desc.access = MemoryAccess::GpuOnly;

            m_meshletData.vertexMeshletIdBuffer = std::make_unique<GPUBuffer>(desc);
            m_meshletData.vertexMeshletIdBuffer->UploadToGPU(vertexMeshletIds.data());
        }

        m_meshletData.meshletCount = static_cast<uint32_t>(meshletCollection.size());
    }
} // namespace Wild
