#include "Tools/Log.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace Wild {
	std::shared_ptr<spdlog::logger> Log::m_logger;

	void Log::initialize() {
		spdlog::set_pattern("%^[%T] %n: %v%$");
		m_logger = spdlog::stdout_color_mt("Wild");
		m_logger->set_level(spdlog::level::trace);
	}
}