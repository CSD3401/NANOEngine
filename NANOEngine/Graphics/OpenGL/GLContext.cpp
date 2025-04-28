#include <glad/glad.h>
#define GLFW_DLL
#include "GLFW/glfw3.h"
#include "GLContext.hpp"
#include "../../Core/Logger.hpp"


namespace NANOEngine::Graphics::OpenGL {


	bool GLContext::Init(void* windowHandle) {
		m_windowHandle = static_cast<GLFWwindow*>(windowHandle);
		//glfwMakeContextCurrent(m_windowHandle);

		//void* proc = glfwGetProcAddress("glGetString");
		//if (!proc) {
		//	LOG_CRITICAL("glfwGetProcAddress failed to get glGetString");
		//}

		//GLFWwindow* current = glfwGetCurrentContext();
		//if (current != m_windowHandle) {
		//	LOG_CRITICAL("OpenGL context is not current before loading GLAD!");
		//	return false;
		//}

		//int api = glfwGetWindowAttrib(m_windowHandle, GLFW_CLIENT_API);
		//if (api != GLFW_OPENGL_API) {
		//	LOG_CRITICAL("GLFW Window is not OpenGL API!");
		//	return false;
		//}

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			const char* error;
			glfwGetError(&error);
			//LOG_CRITICAL("Failed to initialize GLAD: {}", error);
			LOG_CRITICAL(error);
			return false;
			//LOG_CRITICAL("Failed to initialize GLAD");
			//return false;
		}

		LOG_INFO("OpenGL Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

		//printf("OpenGL %s\n", glGetString(GL_VERSION));

		//if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		//	fprintf(stderr, "Failed to initialize GLAD\n");
		//	return false;
		//}

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