#pragma once

#include <cfloat>
#include <cstddef>
#include <glm/glm.hpp>

namespace Wild
{
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

    struct BoundingSphere
    {
        glm::vec3 center{};
        float radius{};

        static BoundingSphere FromBox(const BoundingBox& box) { return {box.GetCenter(), glm::length(box.GetExtents())}; }

        BoundingSphere Transformed(const glm::mat4& matrix) const
        {
            // Largest axis scale keeps the sphere enclosing the mesh under non uniform scale
            const float maxScaleSq = glm::max(glm::max(glm::dot(glm::vec3(matrix[0]), glm::vec3(matrix[0])),
                                                       glm::dot(glm::vec3(matrix[1]), glm::vec3(matrix[1]))),
                                              glm::dot(glm::vec3(matrix[2]), glm::vec3(matrix[2])));

            return {glm::vec3(matrix * glm::vec4(center, 1.0f)), radius * glm::sqrt(maxScaleSq)};
        }
    };

    enum FrustumPlane : std::size_t
    {
        Left,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    class BoundingFrustum
    {
      public:
        BoundingFrustum() = default;

        bool Intersect(const BoundingBox& box) const
        {
            const glm::vec3 c = box.GetCenter();
            const glm::vec3 e = box.GetExtents();

            for (const glm::vec4& p : planes)
            {
                const glm::vec3 n(p);
                const float dist = glm::dot(n, c) + p.w;       // Signed distance of center
                const float radius = glm::dot(e, glm::abs(n)); // Box projected onto normal
                if (dist + radius < 0.0f) return false;        // Behind this plane
            }
            return true;
        }

        bool Intersect(const BoundingSphere& sphere) const
        {
            for (const glm::vec4& p : planes)
            {
                if (glm::dot(glm::vec3(p), sphere.center) + p.w < -sphere.radius) return false; // Fully behind this plane
            }
            return true;
        }

        glm::vec4 planes[Count]{};
    };

} // namespace Wild
