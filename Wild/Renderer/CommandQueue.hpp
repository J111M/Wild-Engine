#pragma once

#include "Renderer/CommandList.hpp"

#include "Tools/Common3d12.hpp"

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

        void WaitForFence() const;

      private:
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        D3D12_COMMAND_LIST_TYPE type{};
    };
} // namespace Wild
