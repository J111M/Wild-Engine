#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::initialize()
	{
		WD_INFO("Wild engine initialized.");

		window = std::make_shared<Window>("Wild engine", 1200, 800);
		device = std::make_shared<Device>(window);
		device->initialize();

		m_ecs = std::make_shared<EntityComponentSystem>();

		m_renderer = std::make_shared<Renderer>();
	}

	void Engine::run()
	{
		auto previousTime = std::chrono::high_resolution_clock::now();
		auto endTime = previousTime;

		auto CameraEntity = engine.GetECS()->CreateEntity();
		engine.GetECS()->AddComponent<Transform>(CameraEntity, glm::vec3(0, 0, -2), CameraEntity);
		auto& camera = engine.GetECS()->AddComponent<Camera>(CameraEntity);

		while (!window->should_close()) {
			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
			previousTime = currentTime;

			camera.Input(*window.get(), window->get_width(), window->get_height(), deltaTime);
			camera.UpdateMatrix(glm::radians(70.0f), window->AspectRatio(), 0.1f, 100.0f);

			device->begin_frame();

			m_renderer->render(*device->GetCommandList().get());

			device->end_frame();

			glfwPollEvents();

			endTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::milli>(endTime - currentTime).count();
		}

		device->flush();

		device->Shutdown();
	}

	void Engine::shutdown()
	{
	}

	Engine engine = {};
}