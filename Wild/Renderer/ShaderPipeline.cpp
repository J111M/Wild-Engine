#include "Renderer/ShaderPipeline.hpp"

namespace Wild
{
    Shader::Shader(const std::string& shaderPath)
    {
        // Check if the shader is a raytracing shader
        std::string rtCheck = shaderPath;
        std::transform(rtCheck.begin(), rtCheck.end(), rtCheck.begin(), ::tolower);
        bool isRaytracingShader = rtCheck.find("ray") != std::string::npos || rtCheck.find("raytracing") != std::string::npos ||
            rtCheck.find("raytrace") != std::string::npos;

        // Check if the shader is a ray tracing shader
        if (isRaytracingShader) { CreateRaytracingShaders(shaderPath); }
        else
        {
            CreateRasterizeShaders(shaderPath);
        }
    }

    std::shared_ptr<RTEntryPoints> Shader::GetRTEntryPoints() const
    {
        if (m_rtEntry)
            return m_rtEntry;
        else
            return nullptr;
    }

    void Shader::CreateRasterizeShaders(const std::string& shaderPath)
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

    void Shader::CreateRaytracingShaders(const std::string& shaderPath)
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

        SlangInt entryPointCount = module->getDefinedEntryPointCount();
        std::vector<ComPtr<slang::IEntryPoint>> entryPoints(entryPointCount);
        std::vector<slang::IComponentType*> components;
        components.push_back(module.Get());

        for (SlangInt i = 0; i < entryPointCount; i++)
        {
            if (SLANG_FAILED(module->getDefinedEntryPoint(i, &entryPoints[i])))
            {
                WD_ERROR("Failed to get entry point {} for shader: '{}'", i, shaderPath.c_str());
            }
            components.push_back(entryPoints[i].Get());
        }

        // Compose
        ComPtr<slang::IComponentType> program;
        if (SLANG_FAILED(
                session->createCompositeComponentType(components.data(), (SlangInt)components.size(), &program, &diagnostics)))
        {
            WD_ERROR("Failed to create slang program for shader: '{}'", shaderPath.c_str());
            if (diagnostics && diagnostics->getBufferSize() > 0)
                WD_ERROR("Diagnostics: {}", (const char*)diagnostics->getBufferPointer());
        }

        // Link
        ComPtr<slang::IComponentType> linkedProgram;
        if (SLANG_FAILED(program->link(&linkedProgram, &diagnostics)))
        {
            WD_ERROR("Failed to link slang program for shader: '{}'", shaderPath.c_str());
            if (diagnostics && diagnostics->getBufferSize() > 0)
                WD_ERROR("Diagnostics: {}", (const char*)diagnostics->getBufferPointer());
        }

        m_rtEntry = std::make_shared<RTEntryPoints>();

        // Get compiled code
        for (SlangInt i = 0; i < entryPointCount; i++)
        {
            EntryPointBlob ep;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(i, 0, &ep.blob, &diagnostics)))
            {
                WD_ERROR("Failed to get entry point code {} for shader: '{}'", i, shaderPath.c_str());
                return;
            }
            ep.name = entryPoints[i]->getFunctionReflection()->getName();
            m_rtEntry->shaderBlobs.push_back(std::move(ep));
        }

        // Shader reflection to get the correct shader stages for the sbt
        slang::ProgramLayout* layout = linkedProgram->getLayout();

        // Loop over all entry points
        for (SlangInt i = 0; i < entryPointCount; i++)
        {
            slang::EntryPointLayout* epLayout = layout->getEntryPointByIndex(i);
            SlangStage stage = epLayout->getStage();

            const char* name = epLayout->getName();
            std::wstring wname(name, name + strlen(name));

            // Push back all the names of the shader stages
            switch (stage)
            {
            case SLANG_STAGE_RAY_GENERATION:
                m_rtEntry->rayGen.push_back(wname);
                break;
            case SLANG_STAGE_CLOSEST_HIT:
                m_rtEntry->closestHit.push_back(wname);
                break;
            case SLANG_STAGE_ANY_HIT:
                m_rtEntry->anyHit.push_back(wname);
                break;
            case SLANG_STAGE_MISS:
                m_rtEntry->miss.push_back(wname);
                break;
            case SLANG_STAGE_INTERSECTION:
                m_rtEntry->intersection.push_back(wname);
                break;
            default:
                break;
            }
        }
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
