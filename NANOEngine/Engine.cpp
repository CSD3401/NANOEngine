#include "Engine.hpp"

#include <memory>
#include "Graphics/Window.hpp"
#include "Graphics/OpenGL/GLContext.hpp"

namespace NANOEngine {

	static std::unique_ptr<Graphics::Window> s_window;
	static std::unique_ptr<Graphics::IRenderContext> s_renderContext;

	void Initialize() {

	}

	void Shutdown() {

	}

	void* GetNativeWindowHandle() {
		return nullptr;
	}
}