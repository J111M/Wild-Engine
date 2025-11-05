#include "Renderer/ShaderPipeline.hpp"

namespace Wild {
	Shader::Shader(std::string shaderPath)
	{
		ComPtr<ID3DBlob> errorBuffer;

		HRESULT hr = D3DCompileFromFile(StringToWString(shaderPath).c_str(),
            nullptr,
            nullptr,
            "main",
            "vs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &m_shaderBlob,
            &errorBuffer);

		if (FAILED(hr)) {
            OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		}

       
        m_shaderBytecode.BytecodeLength = m_shaderBlob->GetBufferSize();
        m_shaderBytecode.pShaderBytecode = m_shaderBlob->GetBufferPointer();


	}
}