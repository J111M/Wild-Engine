#pragma once

#include "Renderer/Resources/Texture.hpp"
#include <Memory>
#include <string>

#include <glm/glm.hpp>

namespace Wild {
	struct Material
	{
		std::shared_ptr<Texture> m_albedo = nullptr;
		std::shared_ptr<Texture> m_normal = nullptr;
		std::shared_ptr<Texture> m_occlusion = nullptr;
		std::shared_ptr<Texture> m_emissive = nullptr;
		std::shared_ptr<Texture> m_roughnessMetallic = nullptr;

		float roughness{};
		float metallic{};
		float emissiveStrenght{};

		glm::vec3 albedo{};
		std::string name{};

		//Material() = default;
	};
}