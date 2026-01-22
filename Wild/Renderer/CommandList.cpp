#include "Renderer/CommandList.hpp"

#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Texture.hpp"
#include "Renderer/Resources/Buffer.hpp"

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

		m_commandList->SetPipelineState(m_pipelineState->GetPso().Get());
	}

	void CommandList::SetBindlessHeap(uint32_t rootIndex) {
		auto context = engine.GetGfxContext();
		if (m_pipelineState->IsComputePass()) {
			m_commandList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIndex), context->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());
		}
		else {
			m_commandList->SetGraphicsRootDescriptorTable(static_cast<UINT>(rootIndex), context->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());
		}

	}

	void CommandList::SetConstantBufferView(uint32_t rootIndex, Buffer* buffer)
	{
		if (m_pipelineState->IsComputePass()) {
			m_commandList->SetComputeRootConstantBufferView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
		else {
			m_commandList->SetGraphicsRootConstantBufferView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
	}

	void CommandList::SetUnorderedAccessView(uint32_t rootIndex, Buffer* buffer)
	{
		if (m_pipelineState->IsComputePass()) {
			m_commandList->SetComputeRootUnorderedAccessView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
		else {
			m_commandList->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
	}

	void CommandList::SetUnorderedAccessView(uint32_t rootIndex, Texture* texture, uint32_t index)
	{
		texture->Transition(*this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		std::shared_ptr<UnorderedAccessView> textureUav;
		// 64 is the default index value TODO rework the array indexing system
		if (index == 64)
			textureUav = texture->GetUav();
		else
			textureUav = texture->GetUav(index);

		if (m_pipelineState->IsComputePass()) {
			m_commandList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIndex), textureUav->GetGpuHandle());
		}
		else {
			m_commandList->SetGraphicsRootDescriptorTable(static_cast<UINT>(rootIndex), textureUav->GetGpuHandle());
		}
	}

	void CommandList::SetShaderResourceView(uint32_t rootIndex, Buffer* buffer)
	{
		if (m_pipelineState->IsComputePass()) {
			m_commandList->SetComputeRootShaderResourceView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
		else {
			m_commandList->SetGraphicsRootShaderResourceView(static_cast<UINT>(rootIndex), buffer->GetBuffer()->GetGPUVirtualAddress());
		}
	}

	void CommandList::BeginRender(const std::vector<Texture*>& renderTargets,
		const std::vector<ClearOperation>& clearRt,
		Texture* depthStencil,
		DSClearOperation clearDs,
		const std::string& passName,
		uint32_t rtArrayIndex)
	{
		if (!CanPassExecute(passName))
			return;

		m_frameInFlight = true;
		m_pipelineIsSet = false;

		auto gfxContext = engine.GetGfxContext();

		m_commandList->SetGraphicsRootSignature(m_pipelineState->GetRootSignature().Get());

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get() };
		m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		auto settings = m_pipelineState->GetPipelineSettings();

		D3D12_VIEWPORT viewPort{};
		if (settings.RasterizerState.Viewport.size.x > 0 && settings.RasterizerState.Viewport.size.y > 0) {
			viewPort.TopLeftX = 0.0f;
			viewPort.TopLeftY = 0.0f;
			viewPort.Width = static_cast<FLOAT>(settings.RasterizerState.Viewport.size.x);
			viewPort.Height = static_cast<FLOAT>(settings.RasterizerState.Viewport.size.y);
			viewPort.MinDepth = 0.0f;
			viewPort.MaxDepth = 1.0f;
		}
		else // By default use window size
		{
			viewPort.TopLeftX = 0.0f;
			viewPort.TopLeftY = 0.0f;
			viewPort.Width = static_cast<FLOAT>(gfxContext->GetWidth());
			viewPort.Height = static_cast<FLOAT>(gfxContext->GetHeight());
			viewPort.MinDepth = 0.0f;
			viewPort.MaxDepth = 1.0f;
		}

		D3D12_RECT scissorRect = { 0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height) };

		m_commandList->RSSetScissorRects(1, &scissorRect);
		m_commandList->RSSetViewports(1, &viewPort);

		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set all reder targets and depth target
		SetRenderTargets(renderTargets, depthStencil, rtArrayIndex);

		// Check if render target's need to be cleared
		for (uint32_t i = 0; i < clearRt.size(); i++)
		{
			if (clearRt[i] == ClearOperation::Clear)
			{
				ClearRenderTarget(*renderTargets[i]);
			}
		}

		// Clear the depth stencil texture depending on the clear flag
		ClearDepthStencil(*depthStencil, clearDs);
	}

	void CommandList::BeginRender(const std::string& passName)
	{
		if (!CanPassExecute(passName))
			return;

		if (!m_pipelineState->IsComputePass())
		{
			WD_WARN("Pass is not a compute pass please provide render targets.");
			return;
		}

		m_frameInFlight = true;
		m_pipelineIsSet = false;

		m_commandList->SetComputeRootSignature(m_pipelineState->GetRootSignature().Get());

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get() };
		m_commandList->SetDescriptorHeaps(1, heaps);
	}

	void CommandList::EndRender()
	{
		//m_commandList->EndEvent();
		m_frameInFlight = false;
	}

	void CommandList::ClearDepthStencil(Texture& depthStencil, const DSClearOperation clear, const float depth, const uint8_t stencil)
	{
		switch (clear)
		{
		case DSClearOperation::DepthClear:
			m_commandList->ClearDepthStencilView(depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
			break;
		case DSClearOperation::StencilClear:
			m_commandList->ClearDepthStencilView(depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
			break;
		case DSClearOperation::ClearAll:
			m_commandList->ClearDepthStencilView(depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
			m_commandList->ClearDepthStencilView(depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
			break;
		default:
			break;
		}
	}

	void CommandList::ClearRenderTarget(Texture& renderTarget, const glm::vec4& color)
	{
		renderTarget.Transition(*this, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ClearRenderTargetView(renderTarget.GetRtv()->GetCpuHandle(), &color[0], 0, nullptr);
	}


	void CommandList::SetRenderTargets(const std::vector<Texture*>& renderTargets, Texture* depthStencil, uint32_t rtArrayIndex)
	{
		const uint32_t numRenderTargets = static_cast<uint32_t>(renderTargets.size());
		if (numRenderTargets >= 8)
			WD_ERROR("Trying to bind more render targets than possible.");

		// Render targets
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
		handles.reserve(numRenderTargets);
		for (size_t i = 0; i < numRenderTargets; i++)
		{
			renderTargets[i]->Transition(*this, D3D12_RESOURCE_STATE_RENDER_TARGET);
			// 64 is the default value for the array index meaning it is not set/used
			if (rtArrayIndex == 64) {
				handles.push_back(renderTargets[i]->GetRtv()->GetCpuHandle());
			}
			else {
				handles.push_back(renderTargets[i]->GetRtv(rtArrayIndex)->GetCpuHandle());
			}
		}

		// Depth stencil
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = nullptr;
		if (depthStencil)
		{
			depthStencil->Transition(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			dsvHandle = &depthStencil->GetDsv()->GetCpuHandle();
		}


		m_commandList->OMSetRenderTargets(numRenderTargets, handles.data(), FALSE, dsvHandle);
	}

	const bool CommandList::CanPassExecute(const std::string& passName)
	{
		if (!m_pipelineIsSet)
		{
			WD_WARN("Invalid PSO or Root signature supplied pass will not execute.");
			return false;
		}

		if (m_frameInFlight)
		{
			WD_WARN("A frame is already in flight on this commandlist call end render first in: %s", passName.c_str());
			return false;
		}

		return true;
	}

}