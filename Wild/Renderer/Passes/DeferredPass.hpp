#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct IndirectFrustum
    {
        // Normalized
        glm::vec4 frustumPlanes[6];
        uint32_t totalObjectCount;
    };

    struct InstanceData
    {
        uint32_t instanceIndex;
        float lodBlend;
    };

    struct IndirectCommand
    {
        UINT drawID;
        D3D12_DRAW_INDEXED_ARGUMENTS drawArgs;
    };

    struct IndirectRendering
    {
    };

    struct RootConstant
    {
        glm::mat4 matrix{};
        glm::mat4 invMatrix{};
        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};
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

      private:
        RootConstant m_rc;
        std::shared_ptr<PipelineState> m_pipeline{};

        std::unique_ptr<Texture> m_texture;

        // Indirect rendering resources
        // Store frustum data
        std::unique_ptr<Buffer> m_frustumBuffer[BACK_BUFFER_COUNT];

        // Keeps track of all instances that need to be culled
        std::shared_ptr<Buffer> m_culledInstancesBuffer[BACK_BUFFER_COUNT];

        // Instance count buffer keeps track of the amount of instances that need to be drawn per LOD
        std::unique_ptr<Buffer> m_automicCounter[BACK_BUFFER_COUNT];

        // Draw command buffer stores the grass blades that need to be drawn via execute indirect
        std::unique_ptr<Buffer> m_drawCommandsBuffer[BACK_BUFFER_COUNT];

        // Command signature for Execute indirect
        ComPtr<ID3D12CommandSignature> m_commandSignature;
    };
} // namespace Wild
