#pragma once

#include "Renderer/Resources/Mesh.hpp"

extern "C"
{
#include "Tools/TangentGeneration/mikktspace.h"
}

namespace Wild
{
    class MikktDataExtractor
    {
      public:
        MikktDataExtractor(std::vector<Vertex>& vertex, std::vector<uint32_t>& index) : m_vertices(vertex), m_indices(index) {}

        // Get number of faces by dividing the indice data
        static int GetNumFaces(const SMikkTSpaceContext* context);

        // Get the num of Vertices faces which is always 3 for since I use triangle but in different applications it could be
        // different
        static int GetNumVerticesOfFace(const SMikkTSpaceContext* /*context*/, const int /*face_index*/);

        // Get and set the data to calculate the tangents
        static void GetPosition(const SMikkTSpaceContext* context, float outPos[], const int face_index, const int vert_index);
        static void GetNormal(const SMikkTSpaceContext* context, float outNormal[], const int face_index, const int vert_index);
        static void GetTexCoord(const SMikkTSpaceContext* context, float outUV[], const int face_index, const int vert_index);
        static void SetTSpaceBasic(const SMikkTSpaceContext* context, const float tangent[], const float bitangent_sign,
                                   const int face_index, const int vert_index);

        static void SetTSpace(const SMikkTSpaceContext* context, const float tangent[], const float bitangent[], const float magS,
                              const float magT, const tbool isOrientationPreserving, const int faceIndex, const int vertIndex);

      private:
        std::vector<Vertex>& m_vertices;
        std::vector<uint32_t>& m_indices;
    };
} // namespace Wild
