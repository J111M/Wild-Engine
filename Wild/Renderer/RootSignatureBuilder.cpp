#include "Renderer/RootSignatureBuilder.hpp"

namespace Wild {
	RootSignatureBuilder& RootSignatureBuilder::AddConstants(uint32_t num32BitValues, uint32_t shaderRegister, uint32_t space, D3D12_SHADER_VISIBILITY visibility)
	{
		auto& param = m_params.emplace_back();
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param.Constants = { shaderRegister, space, num32BitValues };
		param.ShaderVisibility = visibility;
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddCBV(uint32_t shaderRegister, uint32_t space, D3D12_SHADER_VISIBILITY visibility)
	{
		auto& param = m_params.emplace_back();
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor = { shaderRegister, space };
		param.ShaderVisibility = visibility;
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddSRV(uint32_t shaderRegister, uint32_t space, D3D12_SHADER_VISIBILITY visibility)
	{
		auto& param = m_params.emplace_back();
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		param.Descriptor = { shaderRegister, space };
		param.ShaderVisibility = visibility;
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddUAV(uint32_t shaderRegister, uint32_t space, D3D12_SHADER_VISIBILITY visibility)
	{
		auto& param = m_params.emplace_back();
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		param.Descriptor = { shaderRegister, space };
		param.ShaderVisibility = visibility;
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddDescriptorTable(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges, D3D12_SHADER_VISIBILITY visibility)
	{
		m_ranges.push_back(ranges);
		auto& param = m_params.emplace_back();
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
		param.DescriptorTable.pDescriptorRanges = m_ranges.back().data();
		param.ShaderVisibility = visibility;
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddStaticSampler(uint32_t shaderRegister, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode, uint32_t space, D3D12_SHADER_VISIBILITY visibility)
	{
		auto& sampler = m_staticSamplers.emplace_back();
		sampler.Filter = filter;
		sampler.AddressU = addressMode;
		sampler.AddressV = addressMode;
		sampler.AddressW = addressMode;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = shaderRegister;
		sampler.RegisterSpace = space;
		sampler.ShaderVisibility = visibility;
		return *this;
	}

	ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(ID3D12Device* device)
	{
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = static_cast<UINT>(m_params.size());
		desc.pParameters = m_params.data();
		desc.NumStaticSamplers = static_cast<UINT>(m_staticSamplers.size());
		desc.pStaticSamplers = m_staticSamplers.data();
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signature, error;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

		ComPtr<ID3D12RootSignature> rootSignature;
		device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

		if (error)
		{
			const char* errStr = (const char*)error->GetBufferPointer();
			WD_ERROR(errStr);
		}

		return rootSignature;
	}
}