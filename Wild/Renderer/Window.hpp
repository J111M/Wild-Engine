#pragma once

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <string>

namespace Wild {
	class Window
	{
	public:
		Window(std::string window_name, int widthIn, int heightIn);
		~Window();

		bool should_close() { return glfwWindowShouldClose(window); }

		HWND get_handle() { return hwnd; }

		int get_width() { return width; }
		int get_height() { return height; }

		static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
	private:
		int width{}, height{};
		bool m_frameBufferResized = false;

		GLFWwindow* window = nullptr;

		std::string window_name{};
		HWND hwnd{};
	};
}