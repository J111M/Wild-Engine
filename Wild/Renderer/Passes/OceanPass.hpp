#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct OceanPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct OceanRenderRC
    {
        glm::mat4 worldMatix{};
        glm::mat4 invModel{};
    };

    class OceanPass : public RenderFeature
    {
      public:
        OceanPass();
        ~OceanPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        void GenerateOceanPlane(uint32_t resolution = 128);

        OceanRenderRC m_oceanRc{};

        Entity m_chunkEntity;

        // Ocean mesh
        std::unique_ptr<Buffer> m_oceanVertices;
        std::unique_ptr<Buffer> m_oceanIndices;
        uint32_t m_drawCount{};
    };

} // namespace Wild
