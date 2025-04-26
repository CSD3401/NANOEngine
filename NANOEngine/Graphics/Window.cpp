#include "Window.hpp"
#include "../Core/Logger.hpp"

namespace NANOEngine::Graphics {

	struct Window::Impl {
		std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> m_window;

		Impl(const WindowProperties& props) 
			: m_window(nullptr, glfwDestroyWindow) 
		{
			if (!glfwInit()) {
				LOG_CRITICAL("Failed to initialize GLFW");
				return;
			}

			if (props.setHints) {
				props.setHints();
			}

			m_window.reset(glfwCreateWindow(
				props.width, props.height, 
				props.title, 
				nullptr, nullptr));

			if (!m_window) {
				LOG_CRITICAL("Failed to create window");
				glfwTerminate();
				return;
			}
		}
		
		~Impl() {
			glfwTerminate();
		}
	};


	Window::Window(const WindowProperties& props)
		: m_impl(std::make_unique<Impl>(props)) {}

	Window::~Window() = default;

	GLFWwindow* Window::GetWindowHandle() const {
		return m_impl->m_window.get();
	}
}
