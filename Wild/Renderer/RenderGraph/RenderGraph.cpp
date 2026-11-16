#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild
{
    ///
    ///  Graph structure
    ///

    Graph::Graph(TransientResourceCache& initializer) : ResourceInitializer(initializer) {}

    Graph::~Graph()
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(SortedPasses.size()); i++)
        {
            delete SortedPasses[i];
        }

        SortedPasses.clear();
    }

    void Graph::ExecuteGraph()
    {
        auto list = engine.GetGfxContext()->GetCommandList();
        for (auto& pass : SortedPasses)
        {
            if (!pass->IsValid()) continue;

            WD_PROFILESCOPE(pass->GetName());

            pass->ExecuteFunction(*list);
        }

        ResourceInitializer.UpdateLifetimes();
    }

    ///
    /// Graph builder
    ///

    Texture* RenderGraph::CreateTransientTexture(const std::string& key, const TextureDesc& desc)
    {
        return &m_graph->ResourceInitializer.GetOrCreateTexture(key, desc);
    }

    // Kahn's algorithm (https://www.geeksforgeeks.org/dsa/topological-sorting-indegree-based-solution/).
    // Uses a min-heap instead of a FIFO queue so the sort is stable: whenever several passes are ready, the one
    // added first runs first. If the insertion order already satisfies every dependency it is returned unchanged.
    // Returns an empty vector when the graph contains a cycle.
    std::vector<int> RenderGraph::TopologicalSort(int num, std::vector<std::vector<int>>& adj)
    {
        // Create an adjacency list representation of the graph
        std::vector<std::vector<int>> graph(num);

        // Array to store the indegree (number of incoming edges) of each node
        std::vector<int> indegree(num, 0);

        // Build the graph and calculate indegrees
        for (auto& edge : adj)
        {
            graph[edge[0]].push_back(edge[1]); // Directed edge from edge[0] to edge[1]
            indegree[edge[1]]++;               // Increment indegree of the destination node
        }

        // Nodes with indegree 0 (ready to be processed), lowest index first
        std::priority_queue<int, std::vector<int>, std::greater<int>> ready;

        for (int i = 0; i < num; i++)
        {
            if (indegree[i] == 0) { ready.push(i); }
        }

        // Vector to store the topological order
        std::vector<int> topoOrder;
        topoOrder.reserve(num);

        while (!ready.empty())
        {
            const int node = ready.top();
            ready.pop();
            topoOrder.push_back(node);

            // Reduce the indegree of its neighbors
            for (int neighbor : graph[node])
            {
                if (--indegree[neighbor] == 0) { ready.push(neighbor); }
            }
        }

        // Nodes left unprocessed are part of a cycle
        if (static_cast<int>(topoOrder.size()) != num) { return {}; }

        return topoOrder;
    }

    void RenderGraph::SortRenderPasses()
    {
        const uint32_t numPasses = static_cast<uint32_t>(m_graph->Passes.size());

        // Edges as {dependency, dependent} pairs, so a dependency is always ordered before the pass that reads it
        std::vector<std::vector<int>> edges;

        for (int i = 0; i < static_cast<int>(numPasses); i++)
        {
            for (const auto& dependencyType : m_graph->Passes[i]->m_dependencies)
            {
                const auto it = m_graph->PassTypes.find(dependencyType);
                if (it == m_graph->PassTypes.end()) continue;
                if (it->second == i) continue; // A pass reading its own data is not an ordering constraint

                edges.push_back({it->second, i});
            }
        }

        std::vector<int> sortedIndices = TopologicalSort(static_cast<int>(numPasses), edges);

        // On a dependency cycle fall back to insertion order, so every pass still runs (and is deleted) exactly once
        if (sortedIndices.size() != numPasses)
        {
            WD_ERROR("Render graph has a dependency cycle, falling back to insertion order.");

            sortedIndices.resize(numPasses);
            for (uint32_t i = 0; i < numPasses; i++)
                sortedIndices[i] = static_cast<int>(i);
        }

        // Add the sorted passes to the render graph
        m_graph->SortedPasses.clear();
        m_graph->SortedPasses.reserve(numPasses);

        for (const int index : sortedIndices)
        {
            m_graph->SortedPasses.push_back(m_graph->Passes[index]);
        }
    }

    void RenderGraph::CalculateFencePoints()
    {
        const uint32_t numPasses = static_cast<uint32_t>(m_graph->Passes.size());

        // Go over all passes and check their dependencies
        for (uint32_t i = 0; i < numPasses; i++)
        {
            // Check if the sorted pass has a dependency
            if (!m_graph->SortedPasses[i]->m_dependencies.empty()) { m_graph->PassFenceIndices.push_back(i - 1); }
        }

        // Add fence points for syncing
        m_graph->PassFenceIndices.push_back(numPasses - 1);
    }

} // namespace Wild
