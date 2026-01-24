#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <Core/Timer.hpp>
#define GLFW_DLL
#include "glfw/glfw3.h"

namespace Editor {
	class Application {
	public:
		void Init();
		void Run();
		void Exit();

		static bool isRunning;
		static Timer timer;

	private:
		GLFWwindow* window;

		void ShowCursor();
		void HideCursor();
	};
}

#endif