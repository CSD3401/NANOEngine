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
		void Update(double dt);
		void Render(RenderPass pass);
		void ExitScene();
		void SaveScene();
		void SaveSceneIfDirty(const std::string& path = "");

		void BeginPlay();
		void StopPlay();
		bool IsPlaying() const;

		void LoadPrefabScene(const std::string& prefabPath);
		void ClosePrefabScene();

		bool IsEditingPrefab() const { return m_isEditingPrefab; }
		const std::string& GetCurrentPrefabPath() const { return m_prefabPath; }

		Scene* GetActive();
		Scene* GetEditorScene() { return m_editor.get(); }
		Scene* GetPrefabScene() { return m_prefabScene.get(); }

	private:
		std::string m_loadedPath;

		std::unique_ptr<Scene> m_editor;
		std::unique_ptr<Scene> m_runtime;
		std::unique_ptr<Scene> m_prefabScene;

		bool m_isPlaying = false;
		bool m_isEditingPrefab = false;
		std::string m_prefabPath;

		std::vector<uint8_t> m_editorBackup;
	};

}