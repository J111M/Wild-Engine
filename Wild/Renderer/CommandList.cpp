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

	void CommandList::Reset() {
		m_allocator->Reset();
		m_commandList->Reset(m_allocator.Get(), nullptr);
	}

	void CommandList::Close() {
		if (!m_commandListClosed) {
			m_commandList->Close();
			m_commandListClosed = true;
		}
	}

	void CommandList::BeginRender(const std::vector<Texture*>& renderTargets,
								  const std::vector<ClearOperation>& clearRt,
								  Texture* depthStencil,
								  DSClearOperation clearDs,
								  const std::shared_ptr<PipelineState> pipeline)
	{
		if (!pipeline->GetPso() && !pipeline->GetRootSignature()) {
			WD_WARN("Invalid PSO or Root signature supplied pass will not execute.");
			return;
		}

		auto gfxContext = engine.GetGfxContext();

		m_commandList->SetPipelineState(pipeline->GetPso().Get());
		m_commandList->SetGraphicsRootSignature(pipeline->GetRootSignature().Get());

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
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

		m_commandList->OMSetRenderTargets(1, &gfxContext->GetRenderTarget()->GetRtv()->GetCpuHandle(), FALSE, &gfxContext->GetDepthTarget()->GetDsv()->GetCpuHandle());
	}

	void CommandList::EndRender()
	{
	}

}