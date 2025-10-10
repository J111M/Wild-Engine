#include "Renderer/CommandQueue.hpp"

namespace Wild {
    CommandQueue::CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE list_type, const std::string& resource_name) : type{ list_type }
	{
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&command_queue)));
        command_queue->SetName(StringToWString(resource_name).c_str());

        WD_INFO("Command queue create: {0}", resource_name.c_str());
	}
}