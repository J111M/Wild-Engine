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
        for (auto& pass : Passes)
        {
            pass->ExecuteFunction(*list);
        }

        ResourceInitializer.UpdateLifetimes();
    }

    ///
    /// Graph builder
    ///

    Texture *RenderGraph::CreateTransientTexture(const std::string& key, const TextureDesc& desc)
    {
        return &m_graph->ResourceInitializer.GetOrCreateTexture(key, desc);
    }

    // Function from https://www.geeksforgeeks.org/dsa/topological-sorting-indegree-based-solution/
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

        // Queue to store nodes with indegree 0 (ready to be processed)
        std::queue<int> q;

        // Add all nodes with indegree 0 to the queue
        for (int i = 0; i < num; i++)
        {
            if (indegree[i] == 0) { q.push(i); }
        }

        [[maybe_unused]] int processedCount = 0; // To track the number of processed nodes

        // Vector to store the topological order
        std::vector<int> topoOrder;

        // Process nodes in the queue
        while (!q.empty())
        {
            int node = q.front(); // Get the front node
            q.pop();
            topoOrder.push_back(node); // Add the node to the topological order
            processedCount++;          // Increment the count of processed nodes

            // Reduce the indegree of its neighbors
            for (int neighbor : graph[node])
            {
                indegree[neighbor]--; // Remove the edge to the neighbor
                if (indegree[neighbor] == 0)
                {
                    q.push(neighbor); // Add neighbor to the queue
                }
            }
        }

        // CheckMsg(processedCount == n, "Cycle detected: topological sort not possible.");
        return topoOrder; // Return the valid topological order
    }

    void RenderGraph::SortRenderPasses()
    {
        const uint32_t numPasses = static_cast<uint32_t>(m_graph->Passes.size());

        std::vector<std::vector<int>> passes;
        passes.reserve(numPasses);

        // Store all the passes into the new vector container with their dependecy to prepare for the Topological sort
        for (int i = 0; i < static_cast<int>(numPasses); i++)
        {
            if (m_graph->Passes[i]->m_dependencies.empty()) continue;

            std::vector<int> deps;
            for (int j = 0; j < m_graph->Passes[i]->m_dependencies.size(); j++)
            {
                passes.push_back({m_graph->PassTypes[m_graph->Passes[i]->m_dependencies[j]], i});
            }
        }

        std::vector<int> sortedIndices{};
        if (passes.size() > 0) { sortedIndices = TopologicalSort(numPasses, passes); }
        else
        {
            sortedIndices.push_back(0);
        }

        // Add the sorted passes to the render graph
        m_graph->SortedPasses.reserve(numPasses);

        for (uint32_t i = 0; i < numPasses; i++)
        {
            m_graph->SortedPasses.push_back(m_graph->Passes[sortedIndices[i]]);
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
