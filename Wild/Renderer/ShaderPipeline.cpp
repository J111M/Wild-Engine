#include "Renderer/ShaderPipeline.hpp"

namespace Wild {
	Shader::Shader(std::string shaderPath)
	{
		ComPtr<slang::IGlobalSession> globalSession;
		slang::createGlobalSession(&globalSession);

		//char buffer[MAX_PATH];
		//GetModuleFileNameA(NULL, buffer, MAX_PATH);
		//auto path = std::filesystem::path(buffer).parent_path();

		//globalSession->setDownstreamCompilerPath(SLANG_PASS_THROUGH_DXC, path.string().c_str());
		//auto& path = engine.GetSystemPath();

			//auto exePath = std::filesystem::path(argv[0]).parent_path();
		//std::filesystem::path dxcPath = path / "dxc\\bin\\dxc.exe";
		//globalSession->setDownstreamCompilerPath(
		//	SLANG_PASS_THROUGH_DXC,
		//	dxcPath.string().c_str()
		//);

		//// Create specefied dxc path
		//slang::CompilerOptionEntry dxcPath = {};
		//dxcPath.name = slang::CompilerOptionName::DefaultDownstreamCompiler;
		//dxcPath.value.stringValue0 = "dxcompiler.dll";

		/*std::string shaderTarget;
		if (shaderPath.find(".vert") != std::string::npos ||
			shaderPath.find(".vs") != std::string::npos ||
			shaderPath.find("Vert") != std::string::npos) {
			shaderTarget = "vs_6_0";
		}
		else if (shaderPath.find(".frag") != std::string::npos ||
			shaderPath.find(".ps") != std::string::npos ||
			shaderPath.find(".pixel") != std::string::npos ||
			shaderPath.find("Frag") != std::string::npos) {
			shaderTarget = "ps_6_0";
		}
		else if (shaderPath.find(".geom") != std::string::npos ||
			shaderPath.find(".gs") != std::string::npos) {
			shaderTarget = "gs_5_1";
		}
		else if (shaderPath.find(".comp") != std::string::npos ||
			shaderPath.find(".cs") != std::string::npos ||
			shaderPath.find("Compute") != std::string::npos) {
			shaderTarget = "cs_6_0";
		}
		else {
			WD_ERROR("Cannot determine shader type from file: " + shaderPath);
			return;
		}*/

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

		if (diagnostics && diagnostics->getBufferSize() > 0) {
			WD_ERROR("Shader: '{}' could not be loaded: {}", shaderPath.c_str(), (const char*)diagnostics->getBufferPointer());
		}

		ComPtr<slang::IEntryPoint> entryPoint;
		module->findEntryPointByName("main", &entryPoint);

		slang::IComponentType* components[] = { module.Get(), entryPoint.Get() };
		ComPtr<slang::IComponentType> program;
		session->createCompositeComponentType(components, 2, &program, &diagnostics);

	
		program->getEntryPointCode(0, 0, &m_shaderBlob);

		if (SLANG_FAILED(program->getEntryPointCode(0, 0, &m_shaderBlob, &diagnostics))) {
			WD_ERROR("Failed to get entry point code for shader '{}'", shaderPath.c_str());
			if (diagnostics && diagnostics->getBufferSize() > 0) {
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

		if (!shader)
			shader = std::make_shared<Shader>(key);

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

}