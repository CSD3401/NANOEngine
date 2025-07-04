#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "src/Core/Timer.hpp"

namespace Editor {
	class Application {
	public:
		void Init();
		void Run();
		void Exit();

		static bool isRunning;
		Timer timer;
	};
}

#endif