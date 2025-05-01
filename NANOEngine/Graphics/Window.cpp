//#include "Window.hpp"

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

#include "../Core/Logger.hpp"

namespace NANOEngine::Graphics {

    Window::Window(const WindowProperties& props)
        : m_WindowHandle(nullptr), m_Width(props.Width), m_Height(props.Height), m_VSync(props.VSync)
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

        m_WindowHandle = glfwCreateWindow(
            props.Width, props.Height, props.Title, nullptr, nullptr
        );

        if (!m_WindowHandle) {
            LOG_CRITICAL("Failed to create window");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(m_WindowHandle);

        glfwSwapInterval(m_VSync ? 1 : 0);
    }

    void Window::Shutdown() {
        if (m_WindowHandle) {
            glfwDestroyWindow(m_WindowHandle);
            glfwTerminate();
            m_WindowHandle = nullptr;
        }
    }

    void Window::PollEvents() {
        glfwPollEvents();
    }

    void Window::SwapBuffers() {
        glfwSwapBuffers(m_WindowHandle);
    }

    void Window::SetVSync(bool enabled) {
        m_VSync = enabled;
        glfwSwapInterval(m_VSync ? 1 : 0);
    }

    bool Window::IsVSync() const {
        return m_VSync;
    }

    void* Window::GetNativeWindow() const {
        return static_cast<void*>(m_WindowHandle);
    }

}
