#include "Renderer/CommandList.hpp"

namespace Wild {
	CommandList::CommandList(D3D12_COMMAND_LIST_TYPE list_type)
	{
		auto device = engine.get_device();

		type = list_type;

		ThrowIfFailed(device->get_device()->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
		ThrowIfFailed(device->get_device()->CreateCommandList(0, type, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));


		
	}

	void CommandList::reset() {
		allocator->Reset();
		command_list->Reset(allocator.Get(), nullptr);
	}

	void CommandList::close() {
		if (!command_list_closed) {
			command_list->Close();
			command_list_closed = true;
		}
	}

	void CommandList::BeginRender()
	{
		auto device = engine.get_device();

		root_signature_and_pso = false;
		m_frameInFlight = true;

		

		
	}

	void CommandList::EndRender()
	{
	}


}