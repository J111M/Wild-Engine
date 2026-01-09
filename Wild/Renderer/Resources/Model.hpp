#pragma once

#include "Renderer/Device.hpp"
#include "Renderer/Resources/Mesh.hpp"
#include "Renderer/Resources/Material.hpp"

#include "Core/Transform.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>
#include <string>

namespace Wild {
	class Model
	{
	public:
		Model(std::filesystem::path filePath, Entity parent);
		~Model() {};

	private:
		void Load(fastgltf::Asset& model, Entity parent = entt::null);
		void LoadNode(fastgltf::Asset& model, uint32_t nodeIndex, Entity parent);
		void LoadPrimitive(fastgltf::Asset& model, fastgltf::Primitive& primitive, Entity parent);
		void LoadMeshData(fastgltf::Asset& model, fastgltf::Primitive& primitive, std::vector<Vertex>& vertexData, std::vector<uint32_t>& indicesData);
		Material LoadMaterials(fastgltf::Asset& model, fastgltf::Primitive& primitive);

		std::vector<Mesh> m_childMeshes{};

		std::string m_filePath;
	};
}