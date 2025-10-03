#pragma once

#include <spdlog/spdlog.h>

#include <memory>

namespace Wild {
	class Log
	{
	public:
		static void initialize();

		inline static std::shared_ptr<spdlog::logger>& get_logger() { return m_logger; }
	private:

		static std::shared_ptr<spdlog::logger> m_logger;
	};
}

// Log macros code from @Cherno https://youtu.be/dZr-53LAlOw?feature=shared 25:30
#define WD_TRACE(...) ::Wild::Log::get_logger()->trace(__VA_ARGS__)
#define WD_INFO(...)  ::Wild::Log::get_logger()->info(__VA_ARGS__)
#define WD_WARN(...)  ::Wild::Log::get_logger()->warn(__VA_ARGS__)
#define WD_ERROR(...) ::Wild::Log::get_logger()->error(__VA_ARGS__)
#define WD_FATAL(...) ::Wild::Log::get_logger()->critical(__VA_ARGS__)