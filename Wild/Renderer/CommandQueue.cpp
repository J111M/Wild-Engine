#include "Renderer/CommandQueue.hpp"

#include "Renderer/Fence.hpp"

namespace Wild {
    CommandQueue::CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE list_type, const std::string& resource_name) : type{ list_type }
	{
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(StringToWString(resource_name).c_str());

        WD_INFO("Command queue create: {0}", resource_name.c_str());
	}

    void CommandQueue::execute_list(CommandList& list)
    {
        if (list.IsReady()) {
            ID3D12CommandList* l[] = { list.GetList().Get() };
            m_commandQueue->ExecuteCommandLists(1, l);
        }
        else
            WD_ERROR("Command list is not ready for execution but is still called!");

    }

    void CommandQueue::wait_for_fence() const
    {
        Fence fence;
        fence.SignalAndWait(*this);
    }
}