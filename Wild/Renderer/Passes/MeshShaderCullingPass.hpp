#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct DeferredPassData;

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

        // Bindless SRV of the HZB, only used in the late pass
        uint32_t hzbView = INVALID_HEAP_INDEX;
        // Size of HZB mip 0 and the amount of mips, used to pick the mip a bounding sphere covers
        glm::vec<2, uint32_t, glm::packed_highp> hzbSize{};
        uint32_t hzbMipCount{};
    };

    struct DepthMipChainRootConstant
    {
        // Size of the mip that is read from and the mip that is written to
        glm::uvec2 srcSize{};
        glm::uvec2 dstSize{};
    };

    struct HZBPassData
    {
        // Early pass depth the HZB is built from
        Texture* depthTexture;

        // Hierarchical z buffer, used as the occluder data for culling.
        // Mip 0 is the power of two below the screen size holding the farthest depth of the pixels it covers,
        // every other mip stores the farthest depth of its parent
        Texture* hzbTexture;
        uint32_t hzbMipCount;
    };

    /// <summary>
    /// Mesh shader geometry path of the deferred pass.
    /// With the amplification shader it runs two phase meshlet occlusion culling:
    /// phase 1 (early pass) draws the meshlets that were visible last frame, the HZB is built from that depth,
    /// phase 2 (late pass) tests every meshlet against the HZB, draws the newly visible ones and stores the
    /// visibility for the next frame. Without it every meshlet is dispatched straight to the mesh shader.
    /// </summary>
    class MeshShaderCullingPass
    {
      public:
        // Device supports everything the mesh shader path needs
        static bool IsSupported();

        // Adds the early pass and the HZB build. Call before DeferredPassData is allocated,
        // passes execute in the order their data is allocated
        void AddPrePasses(Renderer& renderer, RenderGraph& rg);

        // Adds the pass that writes DeferredPassData: the late pass with amplification, the plain mesh shader pass without
        void AddGeometryPass(Renderer& renderer, RenderGraph& rg);

        // Colours every mesh cluster by its meshlet id
        void SetMeshletDebugView(bool enabled) { m_meshletDebugView = enabled; }

        bool& UseAmplificationShader() { return m_useAmplificationShader; }

        // Stops updating the meshlet visibility so the camera can move around the last culling result
        bool& FreezeCulling() { return m_freezeCulling; }

      private:
        void AddEarlyCullPass(Renderer& renderer, RenderGraph& rg);
        void AddEarlyPass(Renderer& renderer, RenderGraph& rg);
        void AddBuildHZBPass(Renderer& renderer, RenderGraph& rg);
        void AddLateCullPass(Renderer& renderer, RenderGraph& rg);
        void AddLatePass(Renderer& renderer, RenderGraph& rg);
        void AddMeshShaderPass(Renderer& renderer, RenderGraph& rg);

        void RecordMeshShaderDraws(Renderer& renderer, const DeferredPassData& passData, CommandList& list,
                                   const bool useAmplification, const bool latePass, const HZBPassData* hzbData);

        // Total meshlets of every drawable instance, one visibility slot is needed per meshlet per instance
        uint32_t CountSceneMeshlets() const;

        // Grows the visibility buffer when the scene needs more slots, every slot starts out as visible
        void EnsureMeshletVisibilityBuffer(uint32_t meshletCount);

        // Transitions the visibility buffer for the amplification shader, returns its bindless UAV or INVALID_HEAP_INDEX
        uint32_t BindVisibilityBuffer(CommandList& list);

        // Makes the HZB readable and waits for the early pass visibility reads before the late pass overwrites them
        void BeginLatePass(CommandList& list, const HZBPassData& hzbData);

        // Uses the amplification shader to cull meshlets before dispatching the mesh shader
        bool m_useAmplificationShader = true;
        bool m_freezeCulling = false;
        bool m_meshletDebugView = false;

        DepthMipChainRootConstant m_depthMipRc{};

        // Persistent visibility of every meshlet per instance, 1 = visible last frame.
        std::unique_ptr<GPUBuffer> m_meshletVisibilityBuffer;
        uint32_t m_meshletVisibilityCapacity = 0;
    };
} // namespace Wild
