#include <glad/glad.h>
#include "GLContext.hpp"
#include "../../Core/Logger.hpp"


namespace NANOEngine::Graphics::OpenGL {


	bool GLContext::Init(void* windowHandle) {
		m_windowHandle = static_cast<GLFWwindow*>(windowHandle);
		glfwMakeContextCurrent(m_windowHandle);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			fprintf(stderr, "Failed to initialize GLAD\n");
			return false;
		}

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