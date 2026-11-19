#include "Renderer/Fence.hpp"

#include <stdexcept>

namespace Wild
{
    Fence::Fence(ID3D12Device* device)
    {
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "Failed to create fence");

        // Auto-reset event that is signaled by the fence when a waited-on value completes
        m_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_fenceEvent) throw std::runtime_error("Failed to create fence event.");
    }

    Fence::~Fence()
    {
        if (m_fenceEvent) ::CloseHandle(m_fenceEvent);
    }

    uint64_t Fence::Signal(ID3D12CommandQueue* queue)
    {
        const uint64_t value = m_lastSignaledValue + 1;
        ThrowIfFailed(queue->Signal(m_fence.Get(), value), "Failed to signal fence");
        m_lastSignaledValue = value;
        return value;
    }

    void Fence::Wait(uint64_t value)
    {
        if (m_fence->GetCompletedValue() >= value) return;

        ThrowIfFailed(m_fence->SetEventOnCompletion(value, m_fenceEvent), "Failed to set fence completion event");
        ::WaitForSingleObject(m_fenceEvent, INFINITE);
    }
} // namespace Wild
