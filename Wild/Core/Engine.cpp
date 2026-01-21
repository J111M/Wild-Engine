#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::Initialize()
	{
		WD_INFO("Wild engine initialized.");

		m_window = std::make_shared<Window>("Wild engine", 1200, 800);
		m_gfxContext = std::make_shared<GfxContext>(m_window);
		m_gfxContext->Initialize();

		m_ecs = std::make_shared<EntityComponentSystem>();
		m_imguiCore = std::make_shared<ImguiCore>(m_window);
		m_shaderTracker = std::make_shared<ShaderTracker>();

		m_renderer = std::make_shared<Renderer>();

	}

	void Engine::Run()
	{
		auto previousTime = std::chrono::high_resolution_clock::now();
		auto endTime = previousTime;

		auto CameraEntity = engine.GetECS()->CreateEntity();
		auto& transform = engine.GetECS()->AddComponent<Transform>(CameraEntity, glm::vec3(0, 0, 0), CameraEntity);
		auto& camera = engine.GetECS()->AddComponent<Camera>(CameraEntity, glm::vec3(0,0,5));

		float frameTime{};

		while (!m_window->ShouldClose()) {
			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
			previousTime = currentTime;

			camera.Input(*m_window.get(), m_window->GetWidth(), m_window->GetHeight(), deltaTime);
			camera.UpdateMatrix(glm::radians(70.0f), m_window->AspectRatio(), 0.1f, 10000.0f);

			m_gfxContext->ResizeWindow();

			m_gfxContext->BeginFrame();
			m_imguiCore->Prepare();

			m_imguiCore->Watch("Frame time: ", &frameTime);
			m_imguiCore->Watch("delta time: ", &deltaTime);

			// Update and render all the passes
			m_renderer->Update(deltaTime);
			m_renderer->Render(*m_gfxContext->GetCommandList().get(), deltaTime);

			m_imguiCore->Draw();
			m_gfxContext->EndFrame();

			glfwPollEvents();

			endTime = std::chrono::high_resolution_clock::now();
			frameTime = std::chrono::duration<float, std::milli>(endTime - currentTime).count();
		}
	}

	void Engine::Shutdown()
	{
		m_gfxContext->Flush();
		m_renderer.reset();
		m_imguiCore.reset();
		m_ecs.reset();
		m_gfxContext->Shutdown();
		m_gfxContext.reset();
		m_window.reset();
	}

	void Engine::SetSystemPath(const std::filesystem::path& path)
	{
		m_systemPath = path;
	}

	Engine engine = {};
}