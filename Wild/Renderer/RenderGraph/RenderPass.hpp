#pragma once

#include "Renderer/CommandList.hpp"

#include <functional>
#include <queue>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Class insipered by @Tygo Boons https://t-boons.github.io/Rendergraph/

namespace Wild
{
    enum PassType
    {
        Default,
        Graphics,
        Compute,
        MeshShader,
        Raytracing
    };

    class RenderPassBase
    {
      public:
        friend class RenderGraph;
        friend class Graph;

        RenderPassBase() = default;
        RenderPassBase(const RenderPassBase&) = delete;

        virtual ~RenderPassBase() = default;

        const std::string& GetName() const { return m_name; }
        const PassType GetPassType() const { return m_type; }
        virtual void ExecuteFunction(CommandList& commandList) = 0;
        virtual bool IsValid() = 0;

      protected:
        const std::vector<std::type_index>& GetDependencies() const { return m_dependencies; }

        std::vector<std::type_index> m_dependencies;

        std::string m_name;

        PassType m_type = PassType::Default;
    };

    /// <summary>
    /// A pass consists of a lambda that contain the render function
    /// And a data struct which stores data that could be shared with other passes
    /// Inspired by https://logins.github.io/graphics/2021/05/31/RenderGraphs.html
    /// </summary>
    template <typename PassData> class RenderPass : public RenderPassBase
    {
      public:
        friend class RenderGraph;
        friend class Graph;

        virtual void ExecuteFunction(CommandList& commandList) override;
        virtual bool IsValid() override { return m_executeLambda != nullptr; }

      protected:
        RenderPass();

      private:
        PassData m_passData;
        std::function<void(PassData&, CommandList&)> m_executeLambda;
    };

    template <typename PassData> inline RenderPass<PassData>::RenderPass() : m_passData(PassData()) {}

    template <typename PassData> inline void RenderPass<PassData>::ExecuteFunction(CommandList& commandList)
    {
        m_executeLambda(m_passData, commandList);
    }

} // namespace Wild
