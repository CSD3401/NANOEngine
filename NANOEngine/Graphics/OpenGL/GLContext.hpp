#pragma once

#include <GLFW/glfw3.h>
#include "../../NANOEngineAPI.hpp"
#include "../IRenderContext.hpp"

namespace NANOEngine::Graphics::OpenGL {
	class NANOENGINE_API GLContext final : public IRenderContext {
	public:
		bool Init(void* windowHandle) override;
		void Shutdown() override;
		void SwapBuffers() override;
		void ResizeViewport(int width, int height) override;
		void SetWindowTitle(const char* title) override;
	private:
		GLFWwindow* m_windowHandle;
	};
}
