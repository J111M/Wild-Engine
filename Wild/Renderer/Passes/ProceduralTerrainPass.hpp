#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct ProceduralTerrainPassData
	{
		float foo;
	};

	struct TerrainRootConstant {
		glm::vec2 textureSize;
		glm::vec2 chunkPosition;
	};

	class ProceduralTerrainPass : public RenderFeature
	{
	public:
		ProceduralTerrainPass();
		~ProceduralTerrainPass() {};

		virtual void Update(const float dt) override;
		virtual void Add(Renderer& renderer, RenderGraph& rg) override;

		void RegenerateChunks() { m_shouldGenerateChunks = true; }

		std::vector<std::shared_ptr<Texture>> heightMaps;
	private:
		std::vector<uint32_t> m_freedIndices;

		bool m_shouldGenerateChunks = true;

		TerrainRootConstant m_rc{};
	};
}