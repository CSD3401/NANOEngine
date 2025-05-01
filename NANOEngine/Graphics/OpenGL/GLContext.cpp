#include "GLContext.hpp"
#include <glad/glad.h>
#define GLFW_DLL
#include "GLFW/glfw3.h"
#include "../../Core/Logger.hpp"


namespace NANOEngine::Graphics::OpenGL {
	bool GLContext::Init(void* windowHandle) {
		m_windowHandle = static_cast<GLFWwindow*>(windowHandle);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			const char* error;
			glfwGetError(&error);
			LOG_CRITICAL(error);
			return false;
		}

		LOG_INFO("OpenGL Version: ", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

		return true;
	}

	void GLContext::Shutdown() {

	}

	void GLContext::SwapBuffers() {
		glfwSwapBuffers(m_windowHandle);
	}

	void GLContext::ResizeViewport(int , int ) {
		
		//glViewport(0, 0, width, height);
	}

	void GLContext::SetWindowTitle(const char* title) {
		glfwSetWindowTitle(m_windowHandle, title);
	}
}