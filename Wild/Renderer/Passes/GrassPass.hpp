#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#define MAXBLADESPERCHUNK 300000
#define MAXGRASSBLADES 2500000

namespace Wild
{

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

    struct PerBladeComputeRootConstant
    {
        glm::mat4 modelMatrix{};
        glm::vec2 chunkPosition{};
        glm::vec2 minMaxHeight{};
        uint32_t seed{};
        uint32_t chunkId{};
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
        std::shared_ptr<Buffer> CulledBuffer = nullptr;
    };

    struct FrustumBuffer
    {
        glm::mat4 viewProj{};
        glm::vec4 frustumPlanes[6]{};
        glm::vec3 cameraPos{};
        float lod0{};
        float lod1{};
        float lod2{};
        float lodBlendRange{};
        float maxDistance{};
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

    struct SceneData
    {
        glm::mat4 ProjView{};
        glm::vec3 CameraPosition{};
        float windStrength = 4.505f;
        float octaves = 0.51f;
        float frequency = 0.05f;
        float amplitude = 0.385f;
        alignas(16) glm::vec2 windDirection = glm::vec2(2.5f, 1.3f);
        float pad0;
        float pad1;
    };

    struct GrassRC
    {
        glm::mat4 matrix{};
        glm::mat4 invMatrix{};
        uint32_t bladeId{};
        float time;
        uint32_t chunkId{};
        uint32_t terrainView{};
    };

    class GrassPass : public RenderFeature
    {
      public:
        GrassPass();
        ~GrassPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        void AddComputePerBladeDataPass(Renderer& renderer, RenderGraph& rg);

        // Clear counter pass resets the instance count buffer to 0
        void AddClearCounterPass(Renderer& renderer, RenderGraph& rg);
        void AddGrassCulling(Renderer& renderer, RenderGraph& rg);
        void AddIndirectDrawCommandsPass(Renderer& renderer, RenderGraph& rg);
        void AddRenderGrass(Renderer& renderer, RenderGraph& rg);
        void UpdateFrustumData();

        void CreateGrassMeshes();

        // PerBladeCompute data
        PerBladeComputeRootConstant m_pbcrc{};
        std::unique_ptr<Buffer> m_perBladeDataBuffer;
        bool m_recomputeGrassBlades;

        // Store frustum data
        std::unique_ptr<Buffer> m_frustumBuffer;

        // Keeps track of all instances that need to be culled
        std::shared_ptr<Buffer> m_culledInstancesBuffer[BACK_BUFFER_COUNT];

        // Instance count buffer keeps track of the amount of instances that need to be drawn per LOD
        std::unique_ptr<Buffer> m_instanceCountBuffer[BACK_BUFFER_COUNT];

        // Draw command buffer stores the grass blades that need to be drawn via execute indirect
        std::unique_ptr<Buffer> m_drawCommandsBuffer[BACK_BUFFER_COUNT];

        // Command signature for Execute indirect
        ComPtr<ID3D12CommandSignature> m_commandSignature;

        // Data for grass render pass
        GrassRC m_rc{};
        Entity m_chunkEntity;
        float m_accumulatedTime{};
        std::shared_ptr<Buffer> m_sceneData[BACK_BUFFER_COUNT];

        // Contains all LOD's inside the same buffer
        std::unique_ptr<Buffer> m_grassVertices;
        std::unique_ptr<Buffer> m_grassIndices;

        SceneData m_grassSceneData{};

        // 3 grass lod's total
        uint32_t m_lodAmount = 3;
    };
} // namespace Wild
