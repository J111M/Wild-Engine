#include "Renderer/RenderGraph/RenderGraph.hpp"

namespace Wild {
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

	Texture* RenderGraph::CreateTransientTexture(const std::string& key, const TextureDesc& desc)
	{
		return &m_graph->ResourceInitializer.GetOrCreateTexture(key, desc);
	}

	// Function from https://www.geeksforgeeks.org/dsa/topological-sorting-indegree-based-solution/
	std::vector<int> RenderGraph::TopologicalSort(int num, std::vector<std::vector<int>>& adj)
	{
		int n = num;
		std::vector<int> indegree(n, 0);
		std::queue<int> q;
		std::vector<int> list;

		// Compute indegrees
		for (int i = 0; i < n; i++) {
			for (int next : adj[i])
				indegree[next]++;
		}

		// Add all nodes with no dependency into the queue
		for (int i = 0; i < n; i++)
			if (indegree[i] == 0)
				q.push(i);

		// Kahn’s Algorithm (BFS)
		while (!q.empty()) {
			int top = q.front();
			q.pop();
			list.push_back(top);
			for (int next : adj[top]) {
				indegree[next]--;
				if (indegree[next] == 0)
					q.push(next);
			}
		}

		return list;
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
				passes.push_back({ m_graph->PassTypes[m_graph->Passes[i]->m_dependencies[j]], i });
			}
		}

		std::vector<int> sortedIndices = TopologicalSort(numPasses, passes);

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
			if (!m_graph->SortedPasses[i]->m_dependencies.empty())
			{
				m_graph->PassFenceIndices.push_back(i - 1);
			}
		}

		// Add fence points for syncing
		m_graph->PassFenceIndices.push_back(numPasses - 1);
	}
	

}