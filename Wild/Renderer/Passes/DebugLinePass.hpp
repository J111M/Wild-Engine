#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
    struct DebugVertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    struct DebugRootConstant
    {
        glm::mat4 projView{};
    };

    struct DebugLinePassData
    {
        Texture* debugTexture;
    };

    class DebugLinePass : public RenderFeature
    {
      public:
        static constexpr UINT MAX_LINES = 65536;
        static constexpr UINT MAX_VERTS = MAX_LINES * 2;

        void AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color = {1, 1, 1});
        void AddAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color = {1, 1, 1});

        DebugLinePass();
        ~DebugLinePass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        DebugRootConstant m_rc;
        std::vector<DebugVertex> m_lines;

        std::unique_ptr<Buffer> m_lineVertexBuffer;
    };
} // namespace Wild
