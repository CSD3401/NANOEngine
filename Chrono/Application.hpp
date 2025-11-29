#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <Core/Timer.hpp>

namespace Editor {
	class Application {
	public:
		void Init();
		void Run();
		void Exit();

		static bool isRunning;
		static Timer timer;
	};
}

#endif