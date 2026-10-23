#pragma once

#include <cfloat>
#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

namespace Wild
{
    struct BoundingSphere
    {
        glm::vec3 center{};
        float radius{};

        // Sphere around the axis aligned box of the points, in the same space as the points
        static BoundingSphere GenerateFromPoints(const std::vector<glm::vec3>& points)
        {
            if (points.empty()) return {};

            glm::vec3 min{FLT_MAX};
            glm::vec3 max{-FLT_MAX};
            for (const glm::vec3& point : points)
            {
                min = glm::min(min, point);
                max = glm::max(max, point);
            }

            return {(min + max) * 0.5f, glm::length((max - min) * 0.5f)};
        }

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
