#pragma once

#include "Core/Transform.hpp"

#include "Renderer/Resources/Material.hpp"
#include "Renderer/Resources/Meshlets.hpp"

#include <cfloat>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

// struct Material;

namespace Wild
{
    class GPUBuffer;

    struct Vertex
    {
        glm::vec3 position{};
        glm::vec3 color{};
        glm::vec3 normal{};
        glm::vec2 uv{};
        glm::vec4 tangent{};
    };

    // Axis aligned bounding box in mesh local space
    struct BoundingBox
    {
        glm::vec3 min{FLT_MAX};
        glm::vec3 max{-FLT_MAX};

        glm::vec3 GetCenter() const { return (min + max) * 0.5f; }
        glm::vec3 GetExtents() const { return (max - min) * 0.5f; }

        void Expand(const glm::vec3& point)
        {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        // Returns the axis aligned box that encloses this box after transformation
        BoundingBox Transformed(const glm::mat4& matrix) const
        {
            const glm::vec3 center = glm::vec3(matrix * glm::vec4(GetCenter(), 1.0f));
            const glm::mat3 absMatrix(
                glm::abs(glm::vec3(matrix[0])), glm::abs(glm::vec3(matrix[1])), glm::abs(glm::vec3(matrix[2])));
            const glm::vec3 extents = absMatrix * GetExtents();

            return {center - extents, center + extents};
        }
    };

    class Mesh
    {
      public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices = {});
        ~Mesh() {};

        std::shared_ptr<GPUBuffer> GetVertexBuffer() const { return m_vertexBuffer; }
        std::shared_ptr<GPUBuffer> GetIndexBuffer() const { return m_indexBuffer; }

        bool HasIndexBuffer() const { return m_hasIndexBuffer; }

        uint32_t GetVertexCount() const { return m_vertexCount; }
        uint32_t GetDrawCount() const { return m_drawCount; }

        const std::vector<glm::vec3>& GetCollisionPositions() const { return m_collisionPositions; }
        const std::vector<uint32_t>& GetCollisionIndices() const { return m_collisionIndices; }

        const BoundingBox& GetBounds() const { return m_bounds; }

        const Material& GetMaterial() const { return m_material; }
        void SetMaterial(Material& material) { m_material = material; }

        // Ray tracing data is built once per mesh; model copies reuse the
        // BLAS and mesh info and only add their own TLAS instance
        bool HasAccelerationData() const { return m_blasIndex != UINT32_MAX; }
        void SetAccelerationData(uint32_t blasIndex, uint32_t meshInfoIndex)
        {
            m_blasIndex = blasIndex;
            m_meshInfoIndex = meshInfoIndex;
        }
        uint32_t GetBlasIndex() const { return m_blasIndex; }
        uint32_t GetMeshInfoIndex() const { return m_meshInfoIndex; }

        const Meshlets* GetMeshlets() const { return m_meshlets.get(); }

      private:
        std::shared_ptr<GPUBuffer> m_vertexBuffer;
        std::shared_ptr<GPUBuffer> m_indexBuffer;

        Material m_material{};

        bool m_hasIndexBuffer = false;
        uint32_t m_vertexCount{};
        uint32_t m_drawCount{};

        std::vector<glm::vec3> m_collisionPositions;
        std::vector<uint32_t> m_collisionIndices;

        BoundingBox m_bounds{};

        uint32_t m_blasIndex = UINT32_MAX;
        uint32_t m_meshInfoIndex = UINT32_MAX;

        std::unique_ptr<Meshlets> m_meshlets;
    };

    // Mesh component for referencing copied entities
    // to maintain the render structure
    struct MeshComponent
    {
        std::shared_ptr<Mesh> mesh;
    };
} // namespace Wild
