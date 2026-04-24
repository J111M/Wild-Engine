#pragma once

#include "Tools/Common3d12.hpp"

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

    class Shader
    {
      public:
        Shader(const std::string& shaderPath);
        ~Shader() {};

        D3D12_SHADER_BYTECODE GetByteCode() { return m_shaderBytecode; }

        std::shared_ptr<RTEntryPoints> GetRTEntryPoints() const;

      private:
        void CreateRasterizeShaders(const std::string& shaderPath);
        void CreateRaytracingShaders(const std::string& shaderPath);

        std::shared_ptr<RTEntryPoints> m_rtEntry{};

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
