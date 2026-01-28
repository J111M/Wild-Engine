#pragma once

#include "Core/ECS.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <entt/entt.hpp>

#include <string>

namespace Wild
{
    class Transform
    {
      public:
        Transform(glm::vec3 position, Entity& entity);
        Transform(Entity& entity, const glm::vec3& position = {0, 0, 0}, const glm::vec3& scale = {1, 1, 1},
                  const glm::quat& rotation = {})
            : m_entity{entity}, m_position{position}, m_scale{scale}, m_rotation{rotation}
        {
        }
        ~Transform() {};

        void SetPosition(glm::vec3 pos)
        {
            m_position = pos;
            MarkDirty();
        }
        glm::vec3& GetPosition() { return m_position; }

        void SetRotation(glm::quat& rotation)
        {
            m_rotation = rotation;
            MarkDirty();
        }
        glm::quat& GetRotation() { return m_rotation; }

        void SetScale(glm::vec3 scale)
        {
            m_scale = scale;
            MarkDirty();
        }
        glm::vec3& GetScale() { return m_scale; }

        const glm::mat4& GetWorldMatrix();
        void SetFromMatrix(glm::mat4& matrix4);

        // const glm::mat4& GetMatrix() { return m_matrix; }

        bool HasParent() { return m_parent != entt::null; }
        bool HasChild() { return !m_children.empty(); }

        void SetParent(Entity parent);
        Entity& GetParent() { return m_parent; }

        std::vector<Entity>& GetChildren() { return m_children; }
        void RemoveChild(Entity child);

        void MarkDirty();

        std::string Name{};

      private:
        glm::vec3 m_position{0.0f, 0.0f, 0.0f};
        glm::vec3 m_scale{1.0f, 1.0f, 1.0f};
        glm::quat m_rotation = {};

        glm::mat4 m_matrix = {1.0f};

        bool m_matrixUpdated = false;

        Entity& m_entity;
        Entity m_parent{entt::null};
        std::vector<Entity> m_children;
    };
} // namespace Wild
