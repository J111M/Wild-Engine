#pragma once

#include "Tools/Types3d12.hpp"
#include "Tools/Common3d12.hpp"

#include <string>

namespace Wild {
	enum class PipelineStateType : uint8_t
	{
		Graphics,
		Compute
	};

	class PipelineState
	{
	public:
		PipelineState(PipelineStateType Type, const PipelineStateSettings& settings);
		~PipelineState() {};

	private:
		


		ComPtr<ID3D12PipelineState> m_pso;
	};
}