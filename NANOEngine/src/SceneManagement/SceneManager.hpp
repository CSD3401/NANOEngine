#pragma once
#include <memory>
#include <string>
#include "Scene.hpp"

namespace NE::SceneManagement {

	class SceneManager {
	public:
		SceneManager() = default;
		~SceneManager() = default;

		bool LoadScene(const std::string& scenePath);
		void Update(double dt);
		void Render();
		void ExitScene();

		// Fallbacks
		void CreateSceneFallback(const std::string& scenePath);
		void StartSceneFallback();

		void LoadRuntime();
		void StopRuntime();

		bool IsPlaying() const;

		bool LoadPrefabScene(const std::string& prefabPath);
		void ClosePrefabScene();

		bool IsEditingPrefab() const { return m_isEditingPrefab; }
		const std::string& GetCurrentPrefabPath() const { return m_prefabPath; }

		Scene* GetActive();
		Scene* GetEditorScene() { return m_editor.get(); }
		Scene* GetPrefabScene() { return m_prefabScene.get(); }

		// Queue a scene switch to happen at the end of the frame (safe for scripts to call during Update)
		void QueueSceneSwitch(const std::string& scenePath);

	private:
		std::string m_loadedPath;

		std::unique_ptr<Scene> m_editor;
		std::unique_ptr<Scene> m_runtime;
		std::unique_ptr<Scene> m_prefabScene;

		bool m_isPlaying = false;
		bool m_isEditingPrefab = false;
		std::string m_prefabPath;

		// Scene switch queue (processed at end of frame)
		std::string m_queuedScenePath;
		bool m_hasQueuedSceneSwitch = false;
	};

}