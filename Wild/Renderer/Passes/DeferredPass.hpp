#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Passes/MeshShaderCullingPass.hpp"

namespace Wild
{
    struct DeferredRootConstants
    {
        glm::mat4 model{};
        glm::mat4 invTransposeModel{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};

        glm::vec4 baseColor{1.0f};

        uint32_t vertexMeshletIdBufferView{};
        uint32_t meshletDebugView{};
    };

    struct DeferredPassData
    {
        Texture* albedoRoughnessTexture;
        Texture* normalMetallicTexture;
        Texture* emissiveTexture;
        Texture* depthTexture;
    };

    // Same targets as the deferred pass, the late pass owns DeferredPassData so passes reading the G-buffer wait for it
    struct DeferredEarlyPassData : DeferredPassData
    {
    };

    class DeferredPass : public RenderFeature
    {
      public:
        DeferredPass();
        ~DeferredPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void DeferredVertexPass(Renderer& renderer, RenderGraph& rg);

        bool SupportsClusterDebugView() const;

      private:
        DeferredRootConstants m_rc;
        std::shared_ptr<PipelineState> m_pipeline{};

        // Colours every mesh cluster by its meshlet id, works on both geometry paths
        bool m_meshletDebugView = false;

        // Runs the vertex path even when the device supports mesh shaders
        bool m_forceVertexPath = false;

        // Mesh shader geometry path with two phase occlusion culling, used when the device supports it
        MeshShaderCullingPass m_meshShaderCulling;

        std::unique_ptr<Texture> m_texture;

        // Indirect rendering resources
        // Store frustum data
        std::unique_ptr<GPUBuffer> m_frustumBuffer[BACK_BUFFER_COUNT];

        // Keeps track of all instances that need to be culled
        std::shared_ptr<GPUBuffer> m_culledInstancesBuffer[BACK_BUFFER_COUNT];

        // Instance count buffer keeps track of the amount of instances that need to be drawn per LOD
        std::unique_ptr<GPUBuffer> m_automicCounter[BACK_BUFFER_COUNT];

        // Draw command buffer stores the grass blades that need to be drawn via execute indirect
        std::unique_ptr<GPUBuffer> m_drawCommandsBuffer[BACK_BUFFER_COUNT];

        // Command signature for Execute indirect
        ComPtr<ID3D12CommandSignature> m_commandSignature;
    };
} // namespace Wild
