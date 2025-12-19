#pragma once

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <string>

namespace Wild {
	class Window
	{
	public:
		Window(std::string windowName, int width, int height);
		~Window();

		bool ShouldClose() { return glfwWindowShouldClose(m_window); }

		HWND GetHandle() { return m_hwnd; }

		GLFWwindow* GetWindow() { return m_window; }

		int GetWidth() { return m_width; }
		int GetHeight() { return m_height; }

		float AspectRatio() {
			if(m_width > 0 && m_height > 0)
				return  static_cast<float>(m_width) / static_cast<float>(m_height);

			return 0.01;
		}

		static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);
	private:
		int m_width{}, m_height{};
		bool m_frameBufferResized = false;

		GLFWwindow* m_window = nullptr;

		std::string m_windowName{};
		HWND m_hwnd{};
	};
}