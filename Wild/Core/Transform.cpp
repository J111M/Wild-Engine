#include "Core/Transform.hpp"

#include <cassert>

namespace Wild
{
    Transform::Transform(glm::vec3 position, Entity& entity) : m_entity{entity}
    {
        m_position = position;
        m_scale = glm::vec3(1, 1, 1);
        m_rotation = glm::identity<glm::quat>();
    }

    const glm::mat4& Transform::GetWorldMatrix()
    {
        if (m_matrixUpdated)
        {
            const auto position = glm::translate(glm::mat4(1.0f), m_position);
            const auto rotation = glm::toMat4(m_rotation);
            const auto scale = glm::scale(glm::mat4(1.0f), m_scale);

            glm::mat4 localMatrix = position * rotation * scale;

            if (m_parent == entt::null) { m_matrix = position * rotation * scale; }
            else
            {
                auto& registry = engine.GetECS()->GetRegistry();

                assert(registry.valid(m_parent));
                auto parent = registry.get<Transform>(m_parent);

                m_matrix = parent.GetWorldMatrix() * localMatrix;
            }

            m_matrixUpdated = false;
        }

        return m_matrix;
    }

    void Transform::SetFromMatrix(glm::mat4& matrix4)
    {
        m_position.x = matrix4[3][0];
        m_position.y = matrix4[3][1];
        m_position.z = matrix4[3][2];

        m_scale.x = glm::length(glm::vec3(matrix4[0][0], matrix4[0][1], matrix4[0][2]));
        m_scale.y = glm::length(glm::vec3(matrix4[1][0], matrix4[1][1], matrix4[1][2]));
        m_scale.z = glm::length(glm::vec3(matrix4[2][0], matrix4[2][1], matrix4[2][2]));

        glm::mat4 rotation(matrix4[0][0] / m_scale.x,
                           matrix4[0][1] / m_scale.x,
                           matrix4[0][2] / m_scale.z,
                           0,
                           matrix4[1][0] / m_scale.y,
                           matrix4[1][1] / m_scale.y,
                           matrix4[1][2] / m_scale.z,
                           0,
                           matrix4[2][0] / m_scale.z,
                           matrix4[2][1] / m_scale.z,
                           matrix4[2][2] / m_scale.z,
                           0,
                           0,
                           0,
                           0,
                           1);

        m_rotation = glm::quat_cast(rotation);

        MarkDirty();
    }

    void Transform::SetParent(Entity parent)
    {
        auto& registry = engine.GetECS()->GetRegistry();

        // If already has a parent remove it from child
        if (m_parent != entt::null)
        {
            Transform& oldParent = registry.get<Transform>(m_parent);
            oldParent.RemoveChild(m_entity);
        }

        m_parent = parent;

        if (m_parent != entt::null)
        {
            // Add this entity as a child of the new parent
            Transform& parentTransform = registry.get<Transform>(m_parent);
            parentTransform.m_children.push_back(m_entity);
        }

        MarkDirty();
    }

    void Transform::RemoveChild(Entity child)
    {
        m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
    }

    void Transform::MarkDirty()
    {
        m_matrixUpdated = true;

        auto& registry = engine.GetECS()->GetRegistry();
        for (entt::entity child : m_children)
        {
            Transform& childTransform = registry.get<Transform>(child);
            childTransform.MarkDirty();
        }
    }
} // namespace Wild
