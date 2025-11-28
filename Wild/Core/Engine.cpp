#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::Initialize()
	{
		WD_INFO("Wild engine initialized.");

		m_window = std::make_shared<Window>("Wild engine", 1200, 800);
		m_device = std::make_shared<Device>(m_window);
		m_device->Initialize();

		m_ecs = std::make_shared<EntityComponentSystem>();

		m_renderer = std::make_shared<Renderer>();
	}

	void Engine::Run()
	{
		auto previousTime = std::chrono::high_resolution_clock::now();
		auto endTime = previousTime;

		auto CameraEntity = engine.GetECS()->CreateEntity();
		auto& transform = engine.GetECS()->AddComponent<Transform>(CameraEntity, glm::vec3(0, 0, 0), CameraEntity);
		auto& camera = engine.GetECS()->AddComponent<Camera>(CameraEntity, glm::vec3(0,0,5));

		while (!m_window->ShouldClose()) {
			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
			previousTime = currentTime;

			camera.Input(*m_window.get(), m_window->GetWidth(), m_window->GetHeight(), deltaTime);
			camera.UpdateMatrix(glm::radians(70.0f), m_window->AspectRatio(), 0.1f, 100.0f);

			m_device->BeginFrame();

			m_renderer->Render(*m_device->GetCommandList().get(), *m_device->GetComputeCommandList().get());

			m_device->EndFrame();

			glfwPollEvents();

			endTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::milli>(endTime - currentTime).count();
		}
	}

	void Engine::Shutdown()
	{
		m_device->Flush();
		m_renderer.reset();
		m_ecs.reset();
		m_device->Shutdown();
		m_device.reset();
		m_window.reset();
	}

	Engine engine = {};
}