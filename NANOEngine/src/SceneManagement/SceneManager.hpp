#pragma once
#include <string>

namespace NANOEngine::SceneManagement {

	class SceneManager {
	public:
		SceneManager() = default;
		~SceneManager() = default;

		void LoadScene(const std::string& scenePath);
		void UpdateScene(double dt);
		void ExitScene();
		void SaveScene();
		void ReloadScene();
	};

}