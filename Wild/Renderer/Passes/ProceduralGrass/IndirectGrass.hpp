#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
#define MAXBLADESPERCHUNK 200000
#define MAXGRASSBLADES 200000 // 2500000

    /// <summary>
    /// Compute per blade grass data structs
    /// </summary>
    struct PerBladePassData
    {
        float foo;
    };

    struct GrassBladeData
    {
        glm::vec3 worldPosition{};
        float rotation{};
        float height{};
    };

    struct PerBladeComputeRootConstants
    {
        glm::mat4 modelMatrix{};
        glm::vec2 chunkPosition{};
        glm::vec2 minMaxHeight{};
        uint32_t seed{};
        uint32_t chunkId{};
        uint32_t terrainHeightView{};
    };

    /// <summary>
    /// Clear counter data which holds an empty float used as rendergraph indentifier
    /// </summary>
    struct ClearCounterData
    {
        float foo;
    };

    /// <summary>
    /// Data for culling pass and LOD selection
    /// </summary>
    struct GrassCullData
    {
        std::shared_ptr<GPUBuffer> CulledBuffer = nullptr;
    };

    // The frustum and camera position come from the scene data buffer
    struct GrassCullConstants
    {
        float lod0 = 15.0f;
        float lod1 = 30.0f;
        float lod2 = 50.0f;
        float lodBlendRange = 5.0f;
        float maxDistance = 70.0f;
    };

    struct CulledInstance
    {
        uint32_t instanceIndex;
        float lodBlend;
    };

    /// <summary>
    /// Empty indirect command data struct used as rendergraph indentifier
    /// </summary>
    struct IndirectCommandsData
    {
        float foo;
    };

    /// <summary>
    /// Data for rendering the final grass  blades
    /// </summary>
    struct GrassVertex
    {
        glm::vec3 Position{};
        float OneDCoordinates{};
        float Sway{};
    };

    struct RenderGrassData
    {
        Texture* albedoRoughnessTexture;
        Texture* normalMetallicTexture; // Stores SSS
        Texture* emissiveTexture;       // Stores AO
        Texture* depthTexture;
    };

    // Grass and wind settings
    struct GrassWindData
    {
        float windStrength = 4.505f;
        float octaves = 0.51f;
        float frequency = 0.05f;
        float amplitude = 0.385f;
        alignas(16) glm::vec2 windDirection = glm::vec2(2.5f, 1.3f);
        float pad0;
        float pad1;
    };

    // Camera matrices come from the scene data buffer
    struct GrassRootConstants
    {
        glm::mat4 model{};
        glm::mat4 invTransposeModel{};
        uint32_t bladeId{};
        float time;
        uint32_t chunkId{};
        uint32_t terrainView{};
    };

    class IndirectGrass : public RenderFeature
    {
      public:
        IndirectGrass();
        ~IndirectGrass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        void AddComputePerBladeDataPass(Renderer& renderer, RenderGraph& rg);

        // Clear counter pass resets the instance count buffer to 0
        void AddClearCounterPass(Renderer& renderer, RenderGraph& rg);
        void AddGrassCulling(Renderer& renderer, RenderGraph& rg);
        void AddIndirectDrawCommandsPass(Renderer& renderer, RenderGraph& rg);
        void AddRenderGrass(Renderer& renderer, RenderGraph& rg);

        void CreateGrassMeshes();

        // PerBladeCompute data
        PerBladeComputeRootConstants m_pbcrc{};
        std::unique_ptr<GPUBuffer> m_perBladeDataBuffer;
        bool m_recomputeGrassBlades = true;

        // TODO make slider for LOD change in imgui
        GrassCullConstants m_cullConstants{};

        // Keeps track of all instances that need to be culled
        std::shared_ptr<GPUBuffer> m_culledInstancesBuffer[BACK_BUFFER_COUNT];

        // Instance count buffer keeps track of the amount of instances that need to be drawn per LOD
        std::unique_ptr<GPUBuffer> m_instanceCountBuffer[BACK_BUFFER_COUNT];

        // Draw command buffer stores the grass blades that need to be drawn via execute indirect
        std::unique_ptr<GPUBuffer> m_drawCommandsBuffer[BACK_BUFFER_COUNT];

        // Command signature for Execute indirect
        ComPtr<ID3D12CommandSignature> m_commandSignature;

        // Data for grass render pass
        GrassRootConstants m_rc{};
        Entity m_chunkEntity;
        float m_accumulatedTime{};
        std::shared_ptr<GPUBuffer> m_windDataBuffer[BACK_BUFFER_COUNT];

        // Contains all LOD's inside the same buffer
        std::unique_ptr<GPUBuffer> m_grassVertices;
        std::unique_ptr<GPUBuffer> m_grassIndices;

        GrassWindData m_windData{};

        // 3 grass lod's total
        uint32_t m_lodAmount = 3;

        bool m_grassPassEnabled = false;
    };
} // namespace Wild
