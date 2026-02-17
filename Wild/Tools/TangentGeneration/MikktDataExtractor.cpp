#pragma once

#include "Tools/TangentGeneration/MikktDataExtractor.hpp"

namespace Wild
{
    int MikktDataExtractor::GetNumFaces(const SMikkTSpaceContext* context)
    {
        const auto* adapter = static_cast<const MikktDataExtractor*>(context->m_pUserData);
        return static_cast<int>(adapter->m_indices.size()) / 3;
    }

    // Set to 3 but can be different depending on the render pipeline
    int MikktDataExtractor::GetNumVerticesOfFace(const SMikkTSpaceContext* /*context*/, const int /*face_index*/) { return 3; }

    void MikktDataExtractor::GetPosition(const SMikkTSpaceContext* context, float outPos[], const int face_index,
                                         const int vert_index)
    {
        const auto* adapter = static_cast<const MikktDataExtractor*>(context->m_pUserData);
        const uint32_t index = adapter->m_indices[face_index * 3 + vert_index];
        const glm::vec3& pos = adapter->m_vertices[index].position;
        outPos[0] = pos.x;
        outPos[1] = pos.y;
        outPos[2] = pos.z;
    }
    void MikktDataExtractor::GetNormal(const SMikkTSpaceContext* context, float outNormal[], const int face_index,
                                       const int vert_index)
    {
        const auto* adapter = static_cast<const MikktDataExtractor*>(context->m_pUserData);
        const uint32_t index = adapter->m_indices[face_index * 3 + vert_index];
        const glm::vec3& normal = adapter->m_vertices[index].normal;
        outNormal[0] = normal.x;
        outNormal[1] = normal.y;
        outNormal[2] = normal.z;
    }

    void MikktDataExtractor::GetTexCoord(const SMikkTSpaceContext* context, float outUV[], const int face_index,
                                         const int vert_index)
    {
        const auto* adapter = static_cast<const MikktDataExtractor*>(context->m_pUserData);
        const uint32_t index = adapter->m_indices[face_index * 3 + vert_index];
        const glm::vec2& uv = adapter->m_vertices[index].uv;
        outUV[0] = uv.x;
        outUV[1] = uv.y;
    }

    void MikktDataExtractor::SetTSpaceBasic(const SMikkTSpaceContext* context, const float tangent[], const float bitangent_sign,
                                            const int face_index, const int vert_index)
    {
        auto* adapter = static_cast<MikktDataExtractor*>(context->m_pUserData);
        const uint32_t index = adapter->m_indices[face_index * 3 + vert_index];
        adapter->m_vertices[index].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], bitangent_sign);
    }

    void MikktDataExtractor::SetTSpace(const SMikkTSpaceContext* context, const float tangent[], const float bitangent[],
                                       const float magS, const float magT, const tbool isOrientationPreserving,
                                       const int faceIndex, const int vertIndex)
    {
        MikktDataExtractor* adapter = static_cast<MikktDataExtractor*>(context->m_pUserData);
        const uint32_t index = adapter->m_indices[faceIndex * 3 + vertIndex];
        float fSign = isOrientationPreserving ? 1.0f : -1.0f;
        adapter->m_vertices[index].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], fSign);
    }
} // namespace Wild
