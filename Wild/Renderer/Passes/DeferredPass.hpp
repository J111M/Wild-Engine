#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct DeferredMeshShaderRootConstants
    {
        glm::mat4 matrix{};
        glm::mat4 invMatrix{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};

        uint32_t vertexDataBuffer{};
        uint32_t meshletBufferView{};
        uint32_t meshletVertexBufferView{};
        uint32_t primIndexBufferView{};
        uint32_t meshletOffset{};
        uint32_t meshletDebugView{};
        uint32_t meshletCount{};
        uint32_t cullMatrixPadding{};
        glm::mat4 cullMatrix{};
    };

    struct DeferredRootConstants
    {
        glm::mat4 matrix{};
        glm::mat4 invMatrix{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};

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

    class DeferredPass : public RenderFeature
    {
      public:
        DeferredPass();
        ~DeferredPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void IndirectPreparePass(Renderer& renderer, RenderGraph& rg);

        void DeferredMeshShaderPass(Renderer& renderer, RenderGraph& rg, const bool useAmplification);
        void DeferredVertexPass(Renderer& renderer, RenderGraph& rg);

        bool SupportsMeshShaderPath() const;
        bool SupportsClusterDebugView() const;

      private:
        DeferredRootConstants m_rc;
        std::shared_ptr<PipelineState> m_pipeline{};

        // Colours every mesh cluster by its meshlet id, works on both geometry paths
        bool m_meshletDebugView = false;

        // Runs the vertex path even when the device supports mesh shaders
        bool m_forceVertexPath = false;

        // Uses the amplification shader to cull meshlets before dispatching the mesh shader.
        // Only ever takes effect when the mesh shader path is active.
        bool m_useAmplificationShader = true;

        bool m_freezeCulling = false;
        glm::mat4 m_cullViewProjection{1.0f};

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
