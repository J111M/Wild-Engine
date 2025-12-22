#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Renderer/RenderGraph/TransientResourceCache.hpp"

#include "Renderer/Passes/DeferredPass.hpp"

#include "Core/Camera.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	Renderer::Renderer() {
		auto gfxContext = engine.GetGfxContext();

		m_resourceCache = std::make_shared<TransientResourceCache>();

		m_renderFeatures.emplace_back(std::make_unique<DeferredPass>());

		m_grassManager = std::make_unique<GrassManager>();
		m_grassPreCompute = std::make_unique<GrassCompute>();

		m_grassPreCompute->Render(*engine.GetGfxContext()->GetCommandList());
	}

	void Renderer::Update(const float dt)
	{
		for (auto& feature : m_renderFeatures)
		{
			feature->Update(dt);
		}

		// TODO collect all passes and update them
		m_grassManager->Update();
	}

	void Renderer::Render(CommandList& list, float deltaTime) {
		auto gfxContext = engine.GetGfxContext();

		ImGui::Text("Testtext");

		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(gfxContext->GetWidth());
		viewPort.Height = static_cast<FLOAT>(gfxContext->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		
		RenderGraph rg = RenderGraph(*m_resourceCache);

		for (auto& feature : m_renderFeatures)
		{
			feature->Add(*this, rg);
		}

		rg.Compile();
		rg.Execute();

		m_grassManager->Render(list, m_grassPreCompute->GetGrassData(), deltaTime);

		/*list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_grassPreCompute->GetGrassData()->GetBuffer().Get(),
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_COMMON
		));*/

	}

	bool Renderer::HasPipelineInCache(const std::string& key)
	{
		// Count return 1 if a key is availiable
		return m_pipelineCache.find(key) != m_pipelineCache.end();
	}

	std::shared_ptr<PipelineState> Renderer::GetOrCreatePipeline(const std::string& key, PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms)
	{
		if (HasPipelineInCache(key))
			return m_pipelineCache.at(key);

		// If the unordered map doesn't contain a pipeline at hash value create a new one
		auto it = m_pipelineCache.emplace(key, std::make_shared<PipelineState>(Type, settings, uniforms)).first;

		// It contains an std::pair of hash key and pipeline state
		return it->second;
	}

	std::shared_ptr<PipelineState> Renderer::GetPipeline(const std::string& key)
	{
		if (m_pipelineCache.find(key) != m_pipelineCache.end()) {
			return m_pipelineCache.at(key);
		}
		else {
			WD_WARN("Invalid pipeline found of type key: &s", key.c_str());
		}

		return nullptr;
	}
}