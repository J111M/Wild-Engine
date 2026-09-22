#pragma once

#include "meshoptimizer.h"

namespace Wild
{
    struct MeshletData
    {
        std::unique_ptr<GPUBuffer> meshletBuffer;
        std::unique_ptr<GPUBuffer> uniqueVertexIndexBuffer;
        std::unique_ptr<GPUBuffer> primitiveIndexBuffer;
        std::unique_ptr<GPUBuffer> vertexMeshletIdBuffer;

        uint32_t meshletCount = 0;
    };

    struct Meshlet
    {
        glm::vec4 meshletBoundingSphere;

        glm::vec<3, float, glm::packed_highp> coneApex;
        float coneCutoff;

        glm::vec<3, float, glm::packed_highp> coneAxis;
        uint32_t vertexOffset;

        uint32_t meshletVerticesOffset;
        uint32_t meshletTriangleOffset;
        uint32_t meshletVerticesCount;
        uint32_t meshletTriangleCount;
    };

    class Meshlets
    {
      public:
        Meshlets(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices = {},
                 const std::string& meshName = "default");
        ~Meshlets() {};

        // Prevent pointer swapping and nullification
        const MeshletData& GetMeshletData() const { return m_meshletData; }

      private:
        // Meshlet data
        MeshletData m_meshletData{};
    };
} // namespace Wild
