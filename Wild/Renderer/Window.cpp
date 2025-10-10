#include "Renderer/Window.hpp"

#include "Tools/Log.hpp"

#include <Windows.h>
#include <wrl.h>

namespace Wild {
	Window::Window(std::string window_name, int widthIn, int heightIn)
	{
		width = widthIn;
		height = heightIn;

		if (!glfwInit()) {
			WD_FATAL("Failed to initialize glfw!");
			glfwTerminate();
		}
			
		window_name = window_name;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(width, height, window_name.c_str(), nullptr, nullptr);

		hwnd = glfwGetWin32Window(window);
	}

	Window::~Window()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}
}