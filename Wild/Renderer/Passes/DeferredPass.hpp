#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    // Camera matrices come from the scene data buffer, only per object data is stored here
    struct DeferredMeshShaderRootConstants
    {
        glm::mat<4, 4, float, glm::packed_highp> model{};
        glm::mat<4, 4, float, glm::packed_highp> invTransposeModel{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};

        uint32_t vertexDataBuffer{};
        uint32_t meshletBufferView{};
        uint32_t meshletVertexBufferView{};
        uint32_t primIndexBufferView{};

        // Object space position of the culling camera
        glm::vec<3, float, glm::packed_highp> cullCameraPosition{};
        uint32_t meshletCount{};

        uint32_t meshletOffset{};
        uint32_t meshletDebugView{};
    };

    struct DeferredRootConstants
    {
        glm::mat4 model{};
        glm::mat4 invTransposeModel{};
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

    struct DepthMipChainRootConstant
    {
        // Size of the mip that is read from and the mip that is written to
        glm::uvec2 srcSize{};
        glm::uvec2 dstSize{};
    };

    struct OcclusionPrepassData
    {
        // Depth of the previous frame, used as the occluder data for this frame.
        // Mip 0 holds the copied depth, every other mip stores the farthest depth of its parent (hierarchical z buffer)
        Texture* previousDepthTexture;
        uint32_t hzbMipCount;
    };

    struct PreviousDepthCopyPassData
    {
        Texture* depthTexture;
        Texture* previousDepthTexture;
    };

    class DeferredPass : public RenderFeature
    {
      public:
        DeferredPass();
        ~DeferredPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void OcclusionPrepass(Renderer& renderer, RenderGraph& rg);
        void PreviousDepthCopyPass(Renderer& renderer, RenderGraph& rg);

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

        // Depth prepass data
        DepthMipChainRootConstant m_depthMipRc{};
    };
} // namespace Wild
