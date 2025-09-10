#pragma once
#include <memory>
#include <string>
#include "Scene.hpp"

namespace NE::SceneManagement {

	class SceneManager {
	public:
		SceneManager() = default;
		~SceneManager() = default;

		void LoadScene(const std::string& scenePath);
		void ReloadScene();
		void SaveScene();
		void ExitScene();

		void Update(double dt);
		void Render(NE::SceneManagement::RenderPass pass);

		NE::SceneManagement::Scene* GetActive() { return m_active.get(); }

	private:
		std::unique_ptr<NE::SceneManagement::Scene> m_active;
		std::string m_loadedPath;
	};

}