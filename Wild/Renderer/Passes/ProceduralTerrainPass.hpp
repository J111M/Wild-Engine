#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"

namespace Wild
{
    struct TerrainChunk
    {
        std::unique_ptr<Texture> m_heightMap;
    };

    struct GenerateTerrainPassData
    {
        float foo;
    };

    struct GenerateTerrainRootConstant
    {
        glm::vec2 textureSize;
        glm::vec2 chunkPosition;
    };

    struct DrawTerrainPassData
    {
        Texture* albedoRoughnessTexture;
        Texture* normalMetallicTexture; // Stores SSS
        Texture* emissiveTexture;       // Stores AO
        Texture* depthTexture;
    };

    struct DrawTerrainRootConstant
    {
        glm::mat4 worldMatix{};
        glm::mat4 invModel{};
        uint32_t heightMapView{};
    };

    class ProceduralTerrainPass : public RenderFeature
    {
      public:
        ProceduralTerrainPass();
        ~ProceduralTerrainPass() {};

        virtual void Update(const float dt) override;
        virtual void Add(Renderer& renderer, RenderGraph& rg) override;

        void GenerateTerrainPass(Renderer& renderer, RenderGraph& rg);
        void DrawTerrainPass(Renderer& renderer, RenderGraph& rg);

        void RegenerateChunks() { m_shouldGenerateChunks = true; }

        void GenerateTerrainPlane(uint32_t resolution = 128);

      private:
        std::vector<uint32_t> m_freedIndices;

        bool m_shouldGenerateChunks = true;

        GenerateTerrainRootConstant m_grc{};

        // TODO use better data structure for this
        std::vector<Entity> m_terrainChunks;

        Entity m_chunkEntity;
        DrawTerrainRootConstant m_drc{};

        std::unique_ptr<Buffer> m_terrainVertices;
        std::unique_ptr<Buffer> m_terrainIndices;
        uint32_t m_drawCount{};
    };
} // namespace Wild
