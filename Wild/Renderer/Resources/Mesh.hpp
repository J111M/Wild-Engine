#pragma once

#include "Core/Transform.hpp"

#include "Renderer/Resources/Material.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>

class Buffer;
// struct Material;

namespace Wild
{
    struct Vertex
    {
        glm::vec3 position{};
        glm::vec3 color{};
        glm::vec3 normal{};
        glm::vec2 uv{};
        glm::vec4 tangent{};
    };

    class Mesh
    {
      public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices = {});
        ~Mesh() {};

        std::shared_ptr<Buffer> GetVertexBuffer() const { return m_vertexBuffer; }
        std::shared_ptr<Buffer> GetIndexBuffer() const { return m_indexBuffer; }

        bool HasIndexBuffer() const { return m_hasIndexBuffer; }

        uint32_t GetVertexCount() const { return m_vertexCount; }
        uint32_t GetDrawCount() const { return m_drawCount; }

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

      private:
        std::shared_ptr<Buffer> m_vertexBuffer;
        std::shared_ptr<Buffer> m_indexBuffer;

        Material m_material{};

        bool m_hasIndexBuffer = false;
        uint32_t m_vertexCount{};
        uint32_t m_drawCount{};

        uint32_t m_blasIndex = UINT32_MAX;
        uint32_t m_meshInfoIndex = UINT32_MAX;
    };

    // ECS component pointing at a shared Mesh resource. Loading the same model
    // twice hands out the same Mesh (GPU buffers + material) through the mesh
    // ResourceSystem; only the Transform differs per copy.
    struct MeshComponent
    {
        std::shared_ptr<Mesh> mesh;
    };
} // namespace Wild
