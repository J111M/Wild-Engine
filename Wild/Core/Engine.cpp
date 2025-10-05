#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::initialize()
	{
		WD_INFO("Wild engine initialized.");
	}

	void Engine::run()
	{
		while (!m_window.should_close()) {
			glfwPollEvents();
		}
	}

	void Engine::shutdown()
	{
	}

	Engine engine = {};
}