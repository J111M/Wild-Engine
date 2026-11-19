#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/Fence.hpp"

#include "Tools/D3D12Common.hpp"

#include <memory>

namespace Wild
{
    enum class QueueType
    {
        Direct = 0,
        Compute = 1,
        Copy = 2,
        Max = 3
    };

    class CommandQueue
    {
      public:
        CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE listType, const std::string& resourceName);
        ~CommandQueue() {};

        ComPtr<ID3D12CommandQueue> GetQueue() const { return m_commandQueue; }

        D3D12_COMMAND_LIST_TYPE GetType() const { return type; }

        void ExecuteList(CommandList& list);

        /// Queues a fence signal after all work submitted so far and returns its value
        uint64_t Signal() { return m_fence->Signal(m_commandQueue.Get()); }

        /// Blocks the CPU until the GPU has finished the work up to the given fence value
        void WaitForValue(uint64_t value) { m_fence->Wait(value); }

        uint64_t GetCompletedValue() const { return m_fence->GetCompletedValue(); }

        /// Blocks the CPU until all work submitted to this queue so far has finished
        void WaitForFence() { WaitForValue(Signal()); }

      private:
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        D3D12_COMMAND_LIST_TYPE type{};

        // One persistent fence per queue, created once instead of per wait
        std::unique_ptr<Fence> m_fence;
    };
} // namespace Wild
