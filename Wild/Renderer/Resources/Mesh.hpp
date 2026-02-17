#pragma once

#include "Core/Transform.hpp"

#include "Renderer/Resources/Material.hpp"

#include <glm/glm.hpp>
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

        uint32_t GetDrawCount() const { return m_drawCount; }

        const Material& GetMaterial() const { return m_material; }
        void SetMaterial(Material& material) { m_material = material; }

      private:
        std::shared_ptr<Buffer> m_vertexBuffer;
        std::shared_ptr<Buffer> m_indexBuffer;

        Material m_material{};

        bool m_hasIndexBuffer = false;
        uint32_t m_drawCount{};
    };
} // namespace Wild
