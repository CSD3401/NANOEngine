//#include "Window.hpp"
//#include "../Core/Logger.hpp"
//
//namespace NANOEngine::Graphics {
//
//	struct Window::Impl {
//		std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> m_window;
//
//		Impl(const WindowProperties& props) 
//			: m_window(nullptr, glfwDestroyWindow) 
//		{
//			if (!glfwInit()) {
//				LOG_CRITICAL("Failed to initialize GLFW");
//				return;
//			}
//
//			if (props.setHints) {
//				props.setHints();
//			}
//
//			m_window.reset(glfwCreateWindow(
//				props.width, props.height, 
//				props.title, 
//				nullptr, nullptr));
//
//			if (!m_window) {
//				LOG_CRITICAL("Failed to create window");
//				glfwTerminate();
//				return;
//			}
//		}
//		
//		~Impl() {
//			glfwTerminate();
//		}
//	};
//
//
//	Window::Window(const WindowProperties& props)
//		: m_impl(std::make_unique<Impl>(props)) {}
//
//	Window::~Window() = default;
//
//	GLFWwindow* Window::GetWindowHandle() const {
//		return m_impl->m_window.get();
//	}
//}

#include "Window.hpp"
#define GLFW_DLL
#include <GLFW/glfw3.h>

namespace NANOEngine::Graphics {

    Window::Window()
        : m_WindowHandle(nullptr), m_Width(0), m_Height(0), m_VSync(true)
    {
    }

    Window::~Window()
    {
        Shutdown();
    }

    bool Window::Init(GLFWwindow* windowHandle)
    {
        m_WindowHandle = windowHandle;
        if (!m_WindowHandle)
            return false;

        // Get initial size
        glfwGetWindowSize(m_WindowHandle, &m_Width, &m_Height);

        return true;
    }

    void Window::Shutdown()
    {
        m_WindowHandle = nullptr; // EXE will handle actual window destroy
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
    }

    void Window::SwapBuffers()
    {
        glfwSwapBuffers(m_WindowHandle);
    }

    void Window::SetVSync(bool enabled)
    {
        m_VSync = enabled;
        glfwSwapInterval(m_VSync ? 1 : 0);
    }

    bool Window::IsVSync() const
    {
        return m_VSync;
    }

}
