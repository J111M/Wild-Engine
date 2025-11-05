#pragma once

#include "Tools/Common3d12.hpp"

#include <d3dcompiler.h>

#include <map>

namespace Wild {
	class Shader
	{
	public:
		Shader(std::string shaderPath);
		~Shader() {};

		D3D12_SHADER_BYTECODE GetByteCode() { return m_shaderBytecode; }

	private:
		D3D12_SHADER_BYTECODE m_shaderBytecode = {};

		ComPtr<ID3DBlob> m_shaderBlob;
	};
	
	class ShaderTracker
	{
	public:
		ShaderTracker() = default;
		~ShaderTracker() = default;

		void AddOrUpdateShader() {};

	private:
		std::unordered_map<std::string, Shader> m_trackedShaders;
	};

}