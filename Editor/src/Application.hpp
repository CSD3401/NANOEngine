#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "Core/Timer.hpp"
#include "Graphics/Window.hpp"
#include "Graphics/IRenderContext.hpp"

namespace Editor {
	class Application {
	public:
		void Init();
		void Run();
		void Exit();

		static bool isRunning;
		Timer timer;

		//std::unique_ptr<NANOEngine::Graphics::Window> m_appWindow;
		//std::unique_ptr<NANOEngine::Graphics::IRenderContext> m_renderContext;

		GLFWwindow* m_nativeWindow;
	};
}

#endif