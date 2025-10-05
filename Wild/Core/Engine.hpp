#pragma once

#include "Renderer/Window.hpp"

#include <memory>
#include <stdexcept>
#include <vector>
#include <iostream>

#include <chrono>

const uint32_t WIDTH = 1600;
const uint32_t HEIGHT = 1000;

namespace Wild {
	class Engine
	{
	public:
		void initialize();
		void run();
		void shutdown();

	private:
		bool should_close = false;

		Window m_window{ "Wild engine", 1200, 800 };
	};

	extern Engine engine;
}