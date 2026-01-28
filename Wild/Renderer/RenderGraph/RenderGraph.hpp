#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/RenderGraph/RenderPass.hpp"
#include "Renderer/RenderGraph/TransientResourceCache.hpp"

#include "Tools/Log.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Class insipered by @Tygo Boons https://t-boons.github.io/Rendergraph/

namespace Wild
{
    class Graph : public NonCopyable
    {
      public:
        Graph(TransientResourceCache& initializer);
        // Clean up ptr's from the passes
        ~Graph();

        void ExecuteGraph();

        std::vector<RenderPassBase *> SortedPasses;
        std::vector<uint32_t> PassFenceIndices;
        std::vector<RenderPassBase *> Passes;
        std::unordered_map<std::type_index, int> PassTypes;

        TransientResourceCache& ResourceInitializer;

      private:
        friend class RenderGraph;
    };

    class RenderGraph
    {
      public:
        RenderGraph(TransientResourceCache& cache);
        ~RenderGraph() {};

        template <typename PassData>
        void AddPass(const std::string& name, PassType type, std::function<void(PassData&, CommandList&)>&& execute);

        template <typename PassData>
        void AddPass(std::string name, PassType type, std::function<void(const PassData&, CommandList&)>&& execute);

        template <typename PassData> PassData *AllocatePassData();

        template <typename PassType, typename PassData> PassData *GetPassData() const;

        void Compile();
        void Execute();

        template <typename PassData> bool HasPassType() const;

        Texture *CreateTransientTexture(const std::string& key, const TextureDesc& desc);

      private:
        std::vector<int> TopologicalSort(int num, std::vector<std::vector<int>>& adj);

        void SortRenderPasses();
        void CalculateFencePoints();

        template <typename PassData> RenderPass<PassData> *PassFromType() const;

        std::unique_ptr<Graph> m_graph{};

        bool m_isCompiled = false;
    };

    template <typename PassData>
    inline void RenderGraph::AddPass(const std::string& name, PassType type,
                                     std::function<void(PassData&, CommandList&)>&& execute)
    {
        PassFromType<PassData>()->m_executeLambda = execute;
        PassFromType<PassData>()->m_name = name;
        PassFromType<PassData>()->m_type = type;
    }

    template <typename PassData>
    inline void RenderGraph::AddPass(std::string name, PassType type,
                                     std::function<void(const PassData&, CommandList&)>&& execute)
    {
        PassFromType<PassData>()->m_executeLambda = execute;
        PassFromType<PassData>()->m_name = name;
        PassFromType<PassData>()->m_type = type;
    }

    template <typename PassData> inline PassData *RenderGraph::AllocatePassData()
    {
        if (HasPassType<PassData>()) { WD_WARN("Duplicate PassData in pass: {0}", typeid(PassData).name()); }

        // Allocate a new pass of type template
        auto *pass = new RenderPass<PassData>();
        pass->m_name = std::string(typeid(PassData).name()) + " (AddPass has not been called on this pass)";
        m_graph->PassTypes[typeid(PassData)] = static_cast<uint32_t>(m_graph->Passes.size());
        m_graph->Passes.push_back(pass);

        return &pass->m_passData;
    }

    template <typename PassType, typename PassData> inline PassData *RenderGraph::GetPassData() const
    {
        // Get dependency container from PassType
        auto& dependency = PassFromType<PassType>()->m_dependencies;

        // Check if pass already has dependency
        if (std::find(dependency.begin(), dependency.end(), typeid(PassData)) == dependency.end())
        {
            // If not it adds the pass as a dependency to be sorted later.
            dependency.push_back(typeid(PassData));
        }

        // Grab other pass and return it's parameter
        RenderPass<PassData> *pass = PassFromType<PassData>();
        return &pass->m_passData;
    }

    template <typename PassData> inline bool RenderGraph::HasPassType() const
    {
        return m_graph->PassTypes.find(typeid(PassData)) != m_graph->PassTypes.end();
    }

    template <typename PassData> inline RenderPass<PassData> *RenderGraph::PassFromType() const
    {
        if (!HasPassType<PassData>()) { WD_WARN("Pass with type PassData cannot be found: {0}", typeid(PassData).name()); }

        return reinterpret_cast<RenderPass<PassData> *>(m_graph->Passes[m_graph->PassTypes[typeid(PassData)]]);
    }

    inline RenderGraph::RenderGraph(TransientResourceCache& cache) { m_graph = std::make_unique<Graph>(cache); }

    inline void RenderGraph::Compile()
    {
        SortRenderPasses();
        CalculateFencePoints();

        m_isCompiled = true;
    }

    inline void RenderGraph::Execute()
    {
        if (m_isCompiled)
        {
            if (m_graph) m_graph->ExecuteGraph();
        }
        else
            WD_WARN("RenderGraph is not compiled yet, call compile function before execution.");
    }
} // namespace Wild
