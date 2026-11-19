#pragma once

#include "Tools/D3D12Common.hpp"

#include <cstdint>

namespace Wild
{
    class Fence : private NonCopyable
    {
      public:
        explicit Fence(ID3D12Device* device);
        ~Fence();

        /// Queues a signal of the next fence value on the queue and returns that value
        uint64_t Signal(ID3D12CommandQueue* queue);

        /// Blocks the CPU until the GPU has completed the given fence value
        void Wait(uint64_t value);

        uint64_t GetCompletedValue() const { return m_fence->GetCompletedValue(); }
        uint64_t GetLastSignaledValue() const { return m_lastSignaledValue; }

      private:
        ComPtr<ID3D12Fence> m_fence;
        uint64_t m_lastSignaledValue = 0;
        HANDLE m_fenceEvent = nullptr;
    };
} // namespace Wild
