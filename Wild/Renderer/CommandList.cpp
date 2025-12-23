#include "Renderer/CommandList.hpp"

#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Texture.hpp"

namespace Wild {
	CommandList::CommandList(D3D12_COMMAND_LIST_TYPE listType)
	{
		auto gfxContext = engine.GetGfxContext();

		m_type = listType;

		ThrowIfFailed(gfxContext->GetDevice()->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_allocator)));
		ThrowIfFailed(gfxContext->GetDevice()->CreateCommandList(0, m_type, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	}

	CommandList::~CommandList() {
		Close();

		ResetList();
	}

	void CommandList::ResetList() {
		m_allocator->Reset();
		m_commandList->Reset(m_allocator.Get(), nullptr);
		m_pipelineState = nullptr;
		m_pipelineIsSet = false;
		m_commandListClosed = false;
	}

	void CommandList::Close() {
		if (!m_commandListClosed) {
			m_commandList->Close();
			m_commandListClosed = true;
		}
	}

	void CommandList::SetPipelineState(const std::shared_ptr<PipelineState> pipeline)
	{
		m_pipelineIsSet = true;
		m_pipelineState = pipeline;
	}

	void CommandList::BeginRender(const std::vector<Texture*>& renderTargets,
		const std::vector<ClearOperation>& clearRt,
		Texture* depthStencil,
		DSClearOperation clearDs,
		const std::string& passName)
	{
		if (!m_pipelineIsSet)
		{
			WD_WARN("Invalid PSO or Root signature supplied pass will not execute.");
			return;
		}

		if (m_frameInFlight)
		{
			WD_WARN("A frame is already in flight on this commandlist call end render first in: %s", passName.c_str());
			return;
		}

		m_frameInFlight = true;
		m_pipelineIsSet = false;

		//m_commandList->BeginEvent(1u, passName.c_str(), static_cast<uint32_t>(passName.size() + 1));

		

		auto gfxContext = engine.GetGfxContext();

		m_commandList->SetPipelineState(m_pipelineState->GetPso().Get());
		m_commandList->SetGraphicsRootSignature(m_pipelineState->GetRootSignature().Get());

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get() };
		m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		D3D12_VIEWPORT viewPort{};
		viewPort.TopLeftX = 0.0f;
		viewPort.TopLeftY = 0.0f;
		viewPort.Width = static_cast<FLOAT>(gfxContext->GetWidth());
		viewPort.Height = static_cast<FLOAT>(gfxContext->GetHeight());
		viewPort.MinDepth = 0.0f;
		viewPort.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		m_commandList->RSSetScissorRects(1, &scissorRect);
		m_commandList->RSSetViewports(1, &viewPort);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		SetRenderTargets(renderTargets, depthStencil);
	}

	void CommandList::EndRender()
	{
		//m_commandList->EndEvent();
		m_frameInFlight = false;
	}

	void CommandList::SetRenderTargets(const std::vector<Texture*>& renderTargets, Texture* depthStencil)
	{
		const uint32_t numRenderTargets = static_cast<uint32_t>(renderTargets.size());
		if (numRenderTargets >= 8)
			WD_ERROR("Trying to bind more render targets than possible.");

		// Render targets
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		handles.reserve(numRenderTargets);
		for (size_t i = 0; i < numRenderTargets; i++)
		{
			handles.push_back(renderTargets[i]->GetRtv()->GetCpuHandle());
		}

		// Depth stencil
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = nullptr;
		if (depthStencil)
		{
			dsvHandle = &depthStencil->GetDsv()->GetCpuHandle();
		}

		m_commandList->OMSetRenderTargets(numRenderTargets, handles.data(), FALSE, dsvHandle);
	}

}