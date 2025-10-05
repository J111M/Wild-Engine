#include "Renderer/Window.hpp"

#include "Tools/Log.hpp"

#include <Windows.h>
#include <wrl.h>

namespace Wild {
	Window::Window(std::string window_name, int width, int height)
	{
		if (!glfwInit()) {
			WD_FATAL("Failed to initialize glfw!");
			glfwTerminate();
		}
			
		m_window_name = window_name;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_window = glfwCreateWindow(width, height, m_window_name.c_str(), nullptr, nullptr);
	}

	Window::~Window()
	{
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}
}