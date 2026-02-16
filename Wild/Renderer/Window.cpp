#include "Renderer/Window.hpp"

#include "Tools/Log.hpp"

#include <Windows.h>
#include <wrl.h>

namespace Wild
{
    Window::Window(std::string windowName, int width, int height)
    {
        m_width = width;
        m_height = height;

        if (!glfwInit())
        {
            WD_FATAL("Failed to initialize glfw!");
            glfwTerminate();
        }

        m_windowName = windowName;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window = glfwCreateWindow(m_width, m_height, windowName.c_str(), nullptr, nullptr);

        m_hwnd = glfwGetWin32Window(m_window);

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);
    }

    Window::~Window()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void Window::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto userWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

        userWindow->m_frameBufferResized = true;
        userWindow->m_width = width;
        userWindow->m_height = height;
    }
} // namespace Wild
