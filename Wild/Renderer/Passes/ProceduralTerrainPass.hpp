#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"
#include "Renderer/Resources/Texture.hpp"

namespace Wild
{
    struct TerrainChunk
    {
        std::unique_ptr<Texture> heightMap;
        uint32_t id{};
    };

    struct GenerateTerrainPassData
    {
        float foo;
    };

    struct GenerateTerrainRootConstant
    {
        glm::vec2 textureSize;
        glm::vec2 chunkPosition;
        float amplitude = 0.482f;
        float frequency = 0.012f;
        float amplitudeScalar = 0.4f;
        float frequencyScalar = 2.0f;

        float warpStrength = 0.5f;
        float elevationPower = 1.5f;
        float ridgeBlend = 0.4f;
        float waterLevel = -0.11; // -11

        float valleyScalar = 0.3;
        int octaves = 6;
        uint32_t seed = 0;
        float warpScalar = 0.38f;

        float fallOffScalar = 0.92f;
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
        float noiseBlend = 0.155;
        float waterLevel = 0.02f;
    };

    struct TerrainTextures
    {
        uint32_t sandTexture{};
        uint32_t sandNormalTexture{};
        uint32_t sandRoughTexture{};
        uint32_t sandAOTexture{};

        uint32_t rockTexture{};
        uint32_t rockNormalTexture{};
        uint32_t rockRoughTexture{};
        uint32_t rockAOTexture{};

        uint32_t noiseBlendTexture{};
    };

    struct ProjViewCamera
    {
        glm::mat4 m_viewProj{};
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
        bool m_keepGeneratingTerrain = false;

        // TODO remove temp value
        int tempChunkID = 0;

        GenerateTerrainRootConstant m_grc{};

        // TODO use better data structure for this
        std::vector<Entity> m_terrainChunks;

        Entity m_chunkEntity;
        DrawTerrainRootConstant m_drc{};

        std::unique_ptr<Texture> m_albedoTerrainTexture;
        std::unique_ptr<Texture> m_aoTerrainTexture;
        std::unique_ptr<Texture> m_roughnessTerrainTexture;

        // Terrain textures
        std::unique_ptr<Texture> sandTexture;
        std::unique_ptr<Texture> sandNormalTexture;
        std::unique_ptr<Texture> sandRoughTexture;
        std::unique_ptr<Texture> sandAOTexture;

        std::unique_ptr<Texture> grassTexture;
        std::unique_ptr<Texture> grassNormalTexture;
        std::unique_ptr<Texture> grassRoughTexture;
        std::unique_ptr<Texture> grassAOTexture;

        std::unique_ptr<Texture> rockTexture;
        std::unique_ptr<Texture> rockNormalTexture;
        std::unique_ptr<Texture> rockRoughTexture;
        std::unique_ptr<Texture> rockAOTexture;

        std::unique_ptr<Texture> noiseBlendTexture;

        std::unique_ptr<Buffer> m_terrainTexturesCbv;
        std::unique_ptr<Buffer> m_cameraCbv;

        TerrainTextures m_terrainTexturesView{};
        ProjViewCamera m_pvc{};

        // Terrain mesh
        std::unique_ptr<Buffer> m_terrainVertices;
        std::unique_ptr<Buffer> m_terrainIndices;
        uint32_t m_drawCount{};
    };
} // namespace Wild
