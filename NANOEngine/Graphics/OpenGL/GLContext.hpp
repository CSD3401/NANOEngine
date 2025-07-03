#pragma once

#include "../Interfaces/IRenderContext.hpp"

struct GLFWwindow;

namespace NANOEngine::Graphics::OpenGL {
	class GLContext final : public IRenderContext {
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
