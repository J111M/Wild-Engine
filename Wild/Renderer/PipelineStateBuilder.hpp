#pragma once

#include "Tools/States.hpp"
#include "Tools/Common3d12.hpp"

#include <string>

namespace Wild {
	struct Uniform {
		uint32_t ShaderRegister{};
		uint32_t RegisterSpace{};
		RootParams::RootResourceType Type{};
		size_t ResourceSize{};

		D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL;
	};

	enum class PipelineStateType : uint8_t
	{
		Graphics,
		Compute
	};

	class PipelineState
	{
	public:
		PipelineState(PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms);
		~PipelineState() {};

		ComPtr<ID3D12PipelineState> GetPso() { return m_pso; }
		ComPtr<ID3D12RootSignature> GetRootSignature() { return m_rootSignature; }
	private:
		void CreateRootSignature(const std::vector<Uniform>& uniforms);
		void CreateGraphicsPSO();
		void CreateComputePSO();

		PipelineStateType m_type;
		const PipelineStateSettings& m_settings;

		ComPtr<ID3D12PipelineState> m_pso;
		ComPtr<ID3D12RootSignature> m_rootSignature;

		bool m_hasRootConstant = false;
	};
}