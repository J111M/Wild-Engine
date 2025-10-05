#pragma once

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <string>

namespace Wild {
	class Window
	{
	public:
		Window(std::string window_name, int width, int height);
		~Window();

		bool should_close() { return glfwWindowShouldClose(m_window); }

		HWND get_handle() { return m_hwnd; }
	private:

		GLFWwindow* m_window = nullptr;

		std::string m_window_name{};
		HWND m_hwnd{};
	};
}