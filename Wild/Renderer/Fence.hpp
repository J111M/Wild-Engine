#pragma once

#include "Renderer/CommandQueue.hpp"

#include <chrono>

namespace Wild
{
    class Fence : private NonCopyable
    {
      public:
        Fence();
        ~Fence();

        void Signal(const CommandQueue& m_commandQueue);
        void SignalAndWait(const CommandQueue& m_commandQueue);

        void WaitForFenceValue(std::chrono::milliseconds duration = std::chrono::milliseconds::max());

      private:
        ComPtr<ID3D12Fence> m_fence;
        uint64_t m_fenceValue;
        HANDLE m_fenceEvent;
    };
} // namespace Wild
