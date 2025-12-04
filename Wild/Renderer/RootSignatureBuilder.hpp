#pragma once

#include "Tools/Common3d12.hpp"

#include <vector>

namespace Wild {
	class RootSignatureBuilder
	{
	public:
		static const uint32_t MAX_ROOT_PARAMS = 64;

		RootSignatureBuilder& AddConstants(uint32_t num32BitValues, uint32_t shaderRegister, uint32_t space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddCBV(uint32_t shaderRegister, uint32_t space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddSRV(uint32_t shaderRegister, uint32_t space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddUAV(uint32_t shaderRegister, uint32_t space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddDescriptorTable(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

		RootSignatureBuilder& AddStaticSampler(uint32_t shaderRegister,
			D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			uint32_t space = 0,
			D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL);

		ComPtr<ID3D12RootSignature> Build(ID3D12Device* device);
	private:
		std::vector<D3D12_ROOT_PARAMETER> m_params;
		std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> m_ranges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
	};
}