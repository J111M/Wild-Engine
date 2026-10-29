#pragma once

#include "Tools/D3D12Common.hpp"

#include <d3dcompiler.h>
#include <slang.h>

#include <unordered_map>

namespace Wild
{
    struct EntryPointBlob
    {
        ComPtr<slang::IBlob> blob;
        std::string name;
    };

    struct RTEntryPoints
    {
        std::vector<std::wstring> rayGen;
        std::vector<std::wstring> closestHit;
        std::vector<std::wstring> anyHit;
        std::vector<std::wstring> miss;
        std::vector<std::wstring> intersection;

        std::vector<EntryPointBlob> shaderBlobs{};
    };

    // Preprocessor define passed to the shader compiler
    struct ShaderDefine
    {
        std::string name;
        std::string value = "1";
    };

    class Shader
    {
      public:
        Shader(const std::string& shaderPath, const std::vector<ShaderDefine>& defines = {});
        ~Shader() {};

        D3D12_SHADER_BYTECODE GetByteCode() { return m_shaderBytecode; }

        std::shared_ptr<RTEntryPoints> GetRTEntryPoints() const;

      private:
        void CreateRasterizeShaders(const std::string& shaderPath, const std::vector<slang::PreprocessorMacroDesc>& macros);
        void CreateRaytracingShaders(const std::string& shaderPath, const std::vector<slang::PreprocessorMacroDesc>& macros);

        std::shared_ptr<RTEntryPoints> m_rtEntry{};

        D3D12_SHADER_BYTECODE m_shaderBytecode = {};

        ComPtr<slang::IBlob> m_shaderBlob;
    };

    class ShaderTracker
    {
      public:
        ShaderTracker() = default;
        ~ShaderTracker() = default;

        // Every set of defines is cached as its own shader variant
        std::shared_ptr<Shader> GetOrCreateShader(const std::string& key, const std::vector<ShaderDefine>& defines = {});

        void RemoveShader(const std::string& key);
        void ClearAllShaders();

      private:
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_trackedShaders;
    };
} // namespace Wild
