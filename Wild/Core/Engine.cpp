#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild {
	void Engine::initialize()
	{
		WD_INFO("Wild engine initialized.");
	}

	void Engine::run()
	{
		while (!should_close) {

		}
	}

	void Engine::shutdown()
	{
	}

	Engine engine = {};
}