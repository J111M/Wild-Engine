#pragma once

#include "Tools/Common3d12.hpp"

#include <d3dcompiler.h>
#include <slang.h>

#include <unordered_map>

namespace Wild
{
    class Shader
    {
      public:
        Shader(std::string shaderPath);
        ~Shader() {};

        D3D12_SHADER_BYTECODE GetByteCode() { return m_shaderBytecode; }

      private:
        D3D12_SHADER_BYTECODE m_shaderBytecode = {};

        ComPtr<slang::IBlob> m_shaderBlob;
    };

    class ShaderTracker
    {
      public:
        ShaderTracker() = default;
        ~ShaderTracker() = default;

        std::shared_ptr<Shader> GetOrCreateShader(const std::string& key);

        void RemoveShader(const std::string& key);
        void ClearAllShaders();

      private:
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_trackedShaders;
    };
} // namespace Wild
