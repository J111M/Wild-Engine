#pragma once

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
	};

	extern Engine engine;
}