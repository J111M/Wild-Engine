#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
	struct ProceduralTerrainPassData
	{
		std::vector<Texture> heightMaps;
	};

	struct TerrainRootConstant {
		uint32_t emissiveView{};
		uint32_t albedoView{};
		uint32_t normalView{};
		uint32_t depthView{};
	};

	class ProceduralTerrainPass : public RenderFeature
	{
	public:
		ProceduralTerrainPass();
		~ProceduralTerrainPass() {};

		virtual void Update(const float dt) override;
		virtual void Add(Renderer& renderer, RenderGraph& rg) override;

		void RegenerateChunks() { m_shouldGenerateChunks = true; }

	private:
		std::vector<uint32_t> m_freedIndices;

		bool m_shouldGenerateChunks = true;

		TerrainRootConstant m_rc{};
	};
}