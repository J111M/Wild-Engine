#pragma once

#include "Tools/States.hpp"
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

		ComPtr<ID3D12PipelineState> GetPso() { return m_pso; }
	private:
		void CreateGraphicsPSO();

		PipelineStateType m_type;
		const PipelineStateSettings& m_settings;

		ComPtr<ID3D12PipelineState> m_pso;
	};
}