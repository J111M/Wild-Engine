#include "Renderer/CommandList.hpp"

namespace Wild {
	CommandList::CommandList(D3D12_COMMAND_LIST_TYPE list_type)
	{
		auto device = engine.GetDevice();

		m_type = list_type;

		ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_allocator)));
		ThrowIfFailed(device->GetDevice()->CreateCommandList(0, m_type, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));


		
	}

	void CommandList::Reset() {
		m_allocator->Reset();
		m_commandList->Reset(m_allocator.Get(), nullptr);
	}

	void CommandList::Close() {
		if (!command_list_closed) {
			m_commandList->Close();
			command_list_closed = true;
		}
	}

	void CommandList::BeginRender()
	{
		auto device = engine.GetDevice();

		root_signature_and_pso = false;
		m_frameInFlight = true;

		

		
	}

	void CommandList::EndRender()
	{
	}


}