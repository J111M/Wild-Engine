#pragma once

#include "Tools/Common3d12.hpp"

#include <string>

namespace Wild {
	class PipelineStateBuilder
	{
	public:
		PipelineStateBuilder() {};
		~PipelineStateBuilder() {};

	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_psoDesc{};
		std::string m_pipelineName;
	};
}