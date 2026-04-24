#include "Renderer/CommandList.hpp"

#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/Resources/Texture.hpp"

#include <pix3.h>

namespace Wild
{
    CommandList::CommandList(D3D12_COMMAND_LIST_TYPE listType)
    {
        auto gfxContext = engine.GetGfxContext();

        m_type = listType;

        ThrowIfFailed(gfxContext->GetDevice()->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_allocator)));
        ThrowIfFailed(
            gfxContext->GetDevice()->CreateCommandList(0, m_type, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

        // Query feature support of commandlist level 4
        if (engine.GetGfxContext()->GetCapabilities().SupportsRayTracing())
        {
            ThrowIfFailed(m_commandList->QueryInterface(IID_PPV_ARGS(&m_commandList4)));
        }
        else
            WD_INFO("Raytracing features are not supported by the driver");
    }

    CommandList::~CommandList()
    {
        Close();

        ResetList();
    }

    void CommandList::ResetList()
    {
        m_allocator->Reset();
        m_commandList->Reset(m_allocator.Get(), nullptr);
        m_pipelineState = nullptr;
        m_pipelineIsSet = false;
        m_commandListClosed = false;
    }

    void CommandList::Close()
    {
        if (!m_commandListClosed)
        {
            m_commandList->Close();
            m_commandListClosed = true;
        }
    }

    void CommandList::SetPipelineState(const std::shared_ptr<PipelineState> pipeline)
    {
        m_pipelineIsSet = true;
        m_pipelineState = pipeline;

        if (m_pipelineState->GetPassType() == PipelineStateType::Raytracing)
            m_commandList4->SetPipelineState1(m_pipelineState->GetRTPipeline()->GetStateObject().Get());
        else
            m_commandList->SetPipelineState(m_pipelineState->GetPso().Get());
    }

    void CommandList::SetBindlessHeap(uint32_t rootIndex)
    {
        auto context = engine.GetGfxContext();
        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
        case PipelineStateType::MeshPipeline:
            m_commandList->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(rootIndex), context->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRootDescriptorTable(
                static_cast<UINT>(rootIndex), context->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart());
            break;
        }
    }

    void CommandList::SetConstantBufferView(uint32_t rootIndex, Buffer* buffer)
    {
        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
        case PipelineStateType::MeshPipeline:
            m_commandList->SetGraphicsRootConstantBufferView(static_cast<UINT>(rootIndex),
                                                             buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRootConstantBufferView(static_cast<UINT>(rootIndex),
                                                            buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        }
    }

    void CommandList::SetUnorderedAccessView(uint32_t rootIndex, Buffer* buffer)
    {
        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
        case PipelineStateType::MeshPipeline:
            m_commandList->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(rootIndex),
                                                              buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRootUnorderedAccessView(static_cast<UINT>(rootIndex),
                                                             buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        }
    }

    void CommandList::SetUnorderedAccessView(uint32_t rootIndex, Texture* texture, std::optional<uint32_t> index)
    {
        texture->Transition(*this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        std::shared_ptr<UnorderedAccessView> textureUav;

        if (!index.has_value())
            textureUav = texture->GetUav();
        else
            textureUav = texture->GetUav(*index);

        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
        case PipelineStateType::MeshPipeline:
            m_commandList->SetGraphicsRootDescriptorTable(static_cast<UINT>(rootIndex), textureUav->GetGpuHandle());
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIndex), textureUav->GetGpuHandle());
            break;
        }
    }

    void CommandList::SetShaderResourceView(uint32_t rootIndex, Buffer* buffer)
    {

        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Graphics:
        case PipelineStateType::MeshPipeline:
            m_commandList->SetGraphicsRootShaderResourceView(static_cast<UINT>(rootIndex),
                                                             buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        case PipelineStateType::Compute:
            m_commandList->SetComputeRootShaderResourceView(static_cast<UINT>(rootIndex),
                                                            buffer->GetBuffer()->GetGPUVirtualAddress());
            break;
        }
    }

    void CommandList::BeginRender(const std::vector<Texture*>& renderTargets, const std::vector<ClearOperation>& clearRt,
                                  Texture* depthStencil, DSClearOperation clearDs, const std::string& passName,
                                  std::optional<uint32_t> rtArrayIndex)
    {
        if (!CanPassExecute(passName)) return;

        m_frameInFlight = true;
        m_pipelineIsSet = false;

        auto gfxContext = engine.GetGfxContext();

#ifdef DEBUG
        std::wstring wstringPassName(passName.begin(), passName.end());
        // m_commandList->BeginEvent(1, wstringPassName.c_str(), (wstringPassName.size() + 1) * sizeof(wchar_t));
        PIXBeginEvent(m_commandList.Get(), static_cast<UINT32>(GetPassColor(passName)), wstringPassName.c_str());
#endif // DEBUG

        m_commandList->SetGraphicsRootSignature(m_pipelineState->GetRootSignature().Get());

        ID3D12DescriptorHeap* heaps[] = {engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        auto settings = m_pipelineState->GetPipelineSettings();

        D3D12_VIEWPORT viewPort{};
        if (settings.RasterizerState.Viewport.size.x > 0 && settings.RasterizerState.Viewport.size.y > 0)
        {
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

        D3D12_RECT scissorRect = {0u, 0u, static_cast<LONG>(viewPort.Width), static_cast<LONG>(viewPort.Height)};

        m_commandList->RSSetScissorRects(1, &scissorRect);
        m_commandList->RSSetViewports(1, &viewPort);

        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Set all reder targets and depth target
        SetRenderTargets(renderTargets, depthStencil, rtArrayIndex);

        // Check if render target's need to be cleared
        for (uint32_t i = 0; i < clearRt.size(); i++)
        {
            if (clearRt[i] == ClearOperation::Clear) { ClearRenderTarget(*renderTargets[i]); }
        }

        // Clear the depth stencil texture depending on the clear flag
        ClearDepthStencil(*depthStencil, clearDs);
    }

    void CommandList::BeginRender(const std::string& passName)
    {
        if (!CanPassExecute(passName)) return;

        if (m_pipelineState->GetPassType() != PipelineStateType::Compute &&
            m_pipelineState->GetPassType() != PipelineStateType::Raytracing)
        {
            WD_WARN("Pass is not a compute or raytracing pass please provide render targets.");
            return;
        }

#ifdef DEBUG
        std::wstring wstringPassName(passName.begin(), passName.end());
        // m_commandList->BeginEvent(1, wstringPassName.c_str(), (wstringPassName.size() + 1) * sizeof(wchar_t));
        PIXBeginEvent(m_commandList.Get(), static_cast<UINT32>(GetPassColor(passName)), wstringPassName.c_str());
#endif // DEBUG

        m_frameInFlight = true;
        m_pipelineIsSet = false;

        m_commandList->SetComputeRootSignature(m_pipelineState->GetRootSignature().Get());

        ID3D12DescriptorHeap* heaps[] = {engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
        m_commandList->SetDescriptorHeaps(1, heaps);
    }

    void CommandList::EndRender()
    {
#ifdef DEBUG
        PIXEndEvent(m_commandList.Get());
        // m_commandList->EndEvent();
#endif // DEBUG

        if (!m_frameInFlight) WD_WARN("Begin render was never called.");

        m_frameInFlight = false;
    }

    void CommandList::ClearDepthStencil(Texture& depthStencil, const DSClearOperation clear, const float depth,
                                        const uint8_t stencil)
    {
        switch (clear)
        {
        case DSClearOperation::DepthClear:
            m_commandList->ClearDepthStencilView(
                depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
            break;
        case DSClearOperation::StencilClear:
            m_commandList->ClearDepthStencilView(
                depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
            break;
        case DSClearOperation::ClearAll:
            m_commandList->ClearDepthStencilView(
                depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
            m_commandList->ClearDepthStencilView(
                depthStencil.GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
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

    void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        switch (m_pipelineState->GetPassType())
        {
        case PipelineStateType::Compute:
            m_commandList->Dispatch(static_cast<UINT>(x), static_cast<UINT>(y), static_cast<UINT>(z));
            break;
        case PipelineStateType::Raytracing:
            // Feature support is already checked before reaching this point

            auto rtPipeline = m_pipelineState->GetRTPipeline();

            auto& desc = rtPipeline->GetDispatchDesc();
            desc.Width = static_cast<UINT>(x);
            desc.Height = static_cast<UINT>(y);
            desc.Depth = static_cast<UINT>(z);

            m_commandList4->DispatchRays(&desc);
            break;
        }
    }

    void CommandList::SetRenderTargets(const std::vector<Texture*>& renderTargets, Texture* depthStencil,
                                       std::optional<uint32_t> rtArrayIndex)
    {
        const uint32_t numRenderTargets = static_cast<uint32_t>(renderTargets.size());
        if (numRenderTargets >= 8) WD_ERROR("Trying to bind more render targets than possible.");

        // Render targets
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;
        handles.reserve(numRenderTargets);
        for (size_t i = 0; i < numRenderTargets; i++)
        {
            renderTargets[i]->Transition(*this, D3D12_RESOURCE_STATE_RENDER_TARGET);

            if (!rtArrayIndex.has_value()) { handles.push_back(renderTargets[i]->GetRtv()->GetCpuHandle()); }
            else
            {
                handles.push_back(renderTargets[i]->GetRtv(*rtArrayIndex)->GetCpuHandle());
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

    // Random hash to pix color
    uint32_t CommandList::GetPassColor(std::string_view name)
    {
        uint32_t h = 2166136261u;
        for (char c : name)
            h = (h ^ c) * 16777619u;
        uint8_t r = 128 + (h & 0x7F);
        uint8_t g = 128 + ((h >> 8) & 0x7F);
        uint8_t b = 128 + ((h >> 16) & 0x7F);
        return PIX_COLOR(r, g, b);
    }

} // namespace Wild
