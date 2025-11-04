#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::initialize()
	{
		WD_INFO("Wild engine initialized.");

		window = std::make_shared<Window>("Wild engine", 1200, 800);
		device = std::make_shared<Device>(window);
		device->initialize();
	}

	void Engine::run()
	{
		while (!window->should_close()) {
			device->begin_frame();

			device->end_frame();

			glfwPollEvents();
		}

		device->flush();
	}

	void Engine::shutdown()
	{
	}

	Engine engine = {};
}