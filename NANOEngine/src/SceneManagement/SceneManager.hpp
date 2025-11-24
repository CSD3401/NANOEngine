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
		void Update(double dt);
		void Render();
		void ExitScene();
		void SaveScene();
		void SaveSceneIfDirty(const std::string& path = "");

		void BeginPlay();
		void StopPlay();
		bool IsPlaying() const;

		Scene* GetActive();
		Scene* GetEditorScene() { return m_editor.get(); }

	private:
		std::string m_loadedPath;

		std::unique_ptr<Scene> m_editor;
		std::unique_ptr<Scene> m_runtime;
		bool m_isPlaying = false;

		std::vector<uint8_t> m_editorBackup;
	};

}