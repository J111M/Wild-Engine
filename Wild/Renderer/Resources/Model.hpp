#pragma once

#include "Renderer/GfxContext.hpp"
#include "Renderer/Resources/Material.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include "Core/Transform.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>
#include <string>

namespace Wild
{
    class Model
    {
      public:
        Model(std::filesystem::path filePath, Entity parent);
        ~Model() {};

      private:
        void Load(fastgltf::Asset& model, Entity parent = entt::null);
        void LoadNode(fastgltf::Asset& model, uint32_t nodeIndex, Entity parent);
        void LoadPrimitive(fastgltf::Asset& model, fastgltf::Primitive& primitive, Entity parent, const std::string& cacheKey);

        // Returns the shared mesh from the resource system when this geometry
        // was already loaded, otherwise loads it and registers it there
        std::shared_ptr<Mesh> GetOrLoadMesh(fastgltf::Asset& model, fastgltf::Primitive& primitive, const std::string& cacheKey);

        void LoadMeshData(fastgltf::Asset& model, fastgltf::Primitive& primitive, std::vector<Vertex>& vertexData,
                          std::vector<uint32_t>& indicesData);
        Material LoadMaterials(fastgltf::Asset& model, fastgltf::Primitive& primitive);

        void AddMeshToTlas(Mesh& mesh, Transform& transform);

        std::string m_filePath;   // directory the model's textures load from
        std::string m_sourcePath; // full file path, identifies this model in the resource system
    };
} // namespace Wild
