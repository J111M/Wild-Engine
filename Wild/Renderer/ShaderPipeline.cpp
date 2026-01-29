#include "Renderer/ShaderPipeline.hpp"

namespace Wild
{
    Shader::Shader(std::string shaderPath)
    {
        ComPtr<slang::IGlobalSession> globalSession;
        slang::createGlobalSession(&globalSession);

        slang::SessionDesc sessionDesc = {};
        slang::TargetDesc targetDesc = {};

        targetDesc.format = SLANG_DXIL;
        targetDesc.profile = globalSession->findProfile("sm_6_6");

        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;

        slang::ISession* session;
        globalSession->createSession(sessionDesc, &session);

        ComPtr<slang::IBlob> diagnostics;
        ComPtr<slang::IModule> module = session->loadModule(shaderPath.c_str(), &diagnostics);

        if (diagnostics && diagnostics->getBufferSize() > 0)
        {
            WD_ERROR("Shader: '{}' could not be loaded: {}", shaderPath.c_str(), (const char*)diagnostics->getBufferPointer());
        }

        ComPtr<slang::IEntryPoint> entryPoint;
        module->findEntryPointByName("main", &entryPoint);

        if (!entryPoint) WD_ERROR("Failed to find entrypoint for shader: '{}'", shaderPath.c_str());

        slang::IComponentType* components[] = {module.Get(), entryPoint.Get()};
        ComPtr<slang::IComponentType> program;
        if (SLANG_FAILED(session->createCompositeComponentType(components, 2, &program, &diagnostics)))
        {
            WD_ERROR("Failed to create slang program for shader: '{}'", shaderPath.c_str());
            if (diagnostics && diagnostics->getBufferSize() > 0)
            {
                WD_ERROR("Diagnostics: {}", (const char*)diagnostics->getBufferPointer());
            }
        }

        if (SLANG_FAILED(program->getEntryPointCode(0, 0, &m_shaderBlob, &diagnostics)))
        {
            WD_ERROR("Failed to get entry point code for shader: '{}'", shaderPath.c_str());
            if (diagnostics && diagnostics->getBufferSize() > 0)
            {
                WD_ERROR("Diagnostics: {}", (const char*)diagnostics->getBufferPointer());
            }
        }

        m_shaderBytecode.BytecodeLength = m_shaderBlob->getBufferSize();
        m_shaderBytecode.pShaderBytecode = m_shaderBlob->getBufferPointer();
    }

    ///
    /// Shader tracker
    ///

    std::shared_ptr<Shader> ShaderTracker::GetOrCreateShader(const std::string& key)
    {
        auto& shader = m_trackedShaders[key];

        if (!shader) shader = std::make_shared<Shader>(key);

        return shader;
    }

    void ShaderTracker::RemoveShader(const std::string& key)
    {
        // To be implemented
    }

    void ShaderTracker::ClearAllShaders()
    {
        // To be implemented
    }

} // namespace Wild
