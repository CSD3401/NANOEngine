#include "pch.h"
#include "Window.hpp"
#define GLFW_DLL
#include <glfw/glfw3.h>

#include "Core/Logger.hpp"

namespace NE::Graphics {

    Window::Window(const WindowProperties& props)
        : m_windowHandle(nullptr), m_width(props.Width), m_height(props.Height), m_vsync(props.VSync)
    {
        Init(props);
    }

    Window::~Window() {
        Shutdown();
    }

    void Window::Init(const WindowProperties& props) {
        if (!glfwInit()) {
            LOG_CRITICAL("Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_windowHandle = glfwCreateWindow(
            props.Width, props.Height, props.Title, nullptr, nullptr
        );

        if (!m_windowHandle) {
            LOG_CRITICAL("Failed to create window");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(m_windowHandle);

        glfwSwapInterval(m_vsync ? 1 : 0);
        //glfwSwapInterval(0);
    }

    void Window::Shutdown() {
        if (m_windowHandle) {
            glfwDestroyWindow(m_windowHandle);
            glfwTerminate();
            m_windowHandle = nullptr;
        }
    }

    void Window::PollEvents() {
        glfwPollEvents();
    }

    void Window::SwapBuffers() {
        glfwSwapBuffers(m_windowHandle);
    }

    void Window::SetVSync(bool enabled) {
        m_vsync = enabled;
        glfwSwapInterval(m_vsync ? 1 : 0);
    }

    bool Window::IsVSync() const {
        return m_vsync;
    }

    void Window::SetGamma(float gamma) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            glfwSetGamma(monitor, gamma);
        }
    }

    void* Window::GetNativeWindow() const {
        return static_cast<void*>(m_windowHandle);
    }

    bool Window::ShouldClose()
    {
        return glfwWindowShouldClose(m_windowHandle);
    }

}
