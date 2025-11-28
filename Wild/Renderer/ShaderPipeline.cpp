#include "Renderer/ShaderPipeline.hpp"

namespace Wild {
	Shader::Shader(std::string shaderPath)
	{
		ComPtr<ID3DBlob> errorBuffer;

        // Determine type of the shader byname
        std::string shaderTarget;
        if (shaderPath.find(".vert") != std::string::npos ||
            shaderPath.find(".vs") != std::string::npos ||
            shaderPath.find("vert") != std::string::npos) {
            shaderTarget = "vs_5_0";
        }
        else if (shaderPath.find(".frag") != std::string::npos ||
            shaderPath.find(".ps") != std::string::npos ||
            shaderPath.find(".pixel") != std::string::npos ||
            shaderPath.find("frag") != std::string::npos) {
            shaderTarget = "ps_5_0";
        }
        else if (shaderPath.find(".geom") != std::string::npos ||
            shaderPath.find(".gs") != std::string::npos) {
            shaderTarget = "gs_5_0";
        }
        else if (shaderPath.find(".comp") != std::string::npos ||
            shaderPath.find(".cs") != std::string::npos ||
            shaderPath.find("compute") != std::string::npos) {
            shaderTarget = "cs_5_0";
        }
        else {
            WD_ERROR("Cannot determine shader type from file: " + shaderPath);
            return;
        }

		HRESULT hr = D3DCompileFromFile(StringToWString(shaderPath).c_str(),
            nullptr,
            nullptr,
            "main",
            shaderTarget.c_str(),
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &m_shaderBlob,
            &errorBuffer);

		if (FAILED(hr)) {
            OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
            WD_WARN((char*)errorBuffer->GetBufferPointer());
		}

       
        m_shaderBytecode.BytecodeLength = m_shaderBlob->GetBufferSize();
        m_shaderBytecode.pShaderBytecode = m_shaderBlob->GetBufferPointer();


	}
}