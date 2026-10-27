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

        glm::vec<4, float, glm::packed_highp> baseColor{1.0f};

        uint32_t vertexDataBuffer{};
        uint32_t meshletBufferView{};
        uint32_t meshletVertexBufferView{};
        uint32_t primIndexBufferView{};

        // Object space position of the culling camera
        glm::vec<3, float, glm::packed_highp> cullCameraPosition{};
        uint32_t meshletCount{};

        uint32_t meshletOffset{};
        uint32_t meshletDebugView{};

        // First slot of this instance in the meshlet visibility buffer
        uint32_t instanceMeshletOffset{};
        // Bindless UAV of the meshlet visibility buffer, INVALID_HEAP_INDEX when there is none
        uint32_t meshletVisibilityView = INVALID_HEAP_INDEX;
    };

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

    struct DepthMipChainRootConstant
    {
        // Size of the mip that is read from and the mip that is written to
        glm::uvec2 srcSize{};
        glm::uvec2 dstSize{};
    };

    struct HZBPassData
    {
        // Hierarchical z buffer, used as the occluder data for culling.
        // Mip 0 holds the copied depth, every other mip stores the farthest depth of its parent (hierarchical z buffer)
        Texture* hzbTexture;
        uint32_t hzbMipCount;
    };

    struct HZBRootConstant
    {
        // Size of the mip that is read from and the mip that is written to
        glm::uvec2 srcSize{};
        glm::uvec2 dstSize{};
    };

    struct HZBCullingPassData
    {
        // Hierarchical z buffer, used as the occluder data for culling.
        // Mip 0 holds the copied depth, every other mip stores the farthest depth of its parent (hierarchical z buffer)
        Texture* hzbTexture;
        uint32_t hzbMipCount;
    };

    struct HZBDepthCopyPassData
    {
        Texture* depthTexture;
        Texture* hzbTexture;
    };

    class DeferredPass : public RenderFeature
    {
      public:
        DeferredPass();
        ~DeferredPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void DeferredEarlyPass(Renderer& renderer, RenderGraph& rg);
        void BuildHZBPass(Renderer& renderer, RenderGraph& rg);
        void DeferredLatePass(Renderer& renderer, RenderGraph& rg);

        void HZBDepthCopyPass(Renderer& renderer, RenderGraph& rg);

        void DeferredMeshShaderPass(Renderer& renderer, RenderGraph& rg, const bool useAmplification);
        void DeferredVertexPass(Renderer& renderer, RenderGraph& rg);

        bool SupportsMeshShaderPath() const;
        bool SupportsClusterDebugView() const;

        // Total meshlets of every drawable instance, one visibility slot is needed per meshlet per instance
        uint32_t CountSceneMeshlets() const;
        // Grows the visibility buffer when the scene needs more slots, every slot starts out as visible
        void EnsureMeshletVisibilityBuffer(uint32_t meshletCount);

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

        // Persistent visibility of every meshlet per instance, 1 = visible last frame.
        std::unique_ptr<GPUBuffer> m_meshletVisibilityBuffer;
        uint32_t m_meshletVisibilityCapacity = 0;
    };
} // namespace Wild
