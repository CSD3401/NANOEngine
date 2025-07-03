#ifndef NANOENGINE_GRAPHICS_IRENDERCONTEXT_HPP
#define NANOENGINE_GRAPHICS_IRENDERCONTEXT_HPP

#include "../../NANOEngineAPI.hpp"

namespace NANOEngine::Graphics {
	enum class RenderAPI { OpenGL, Vulkan };

	class IRenderContext {
	public:
		virtual ~IRenderContext() = default;
		virtual bool Init(void* windowHandle) = 0;
		virtual void Shutdown() = 0;
		virtual void SwapBuffers() = 0;
		virtual void ResizeViewport(int width, int height) = 0;
		virtual void SetWindowTitle(const char* title) = 0;
	};
}

#endif // !NANOENGINE_GRAPHICS_IRENDERCONTEXT_HPP