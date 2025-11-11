#include "SceneManager.hpp"
#include "Serialisation/JsonSceneSerializer.hpp"

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		NE::Serialization::JsonSceneSerializer::Deserialize(*m_editor, path);
		m_editor->Init();
		m_isPlaying = false;
		m_runtime.reset();
	}

	void SceneManager::ReloadScene() {
		if (!m_editor || m_loadedPath.empty()) return;
		m_editor->Exit();
		m_editor = std::make_unique<NE::SceneManagement::Scene>();
		m_editor->Init();
		Serialization::JsonSceneSerializer::Deserialize(*m_editor, m_loadedPath);
	}

	void SceneManager::SaveScene() {
		//if (!m_active) return;
		//Serialization::JsonSceneSerializer::Serialize(*m_active, path);
	}

	void SceneManager::BeginPlay() {
		if (!m_editor || m_isPlaying) return;

		// 1) serialize editor scene to memory
		m_editorBackup.clear();
		NE::Serialization::JsonSceneSerializer::SerializeToMemory(*m_editor, m_editorBackup);

		// 2) create runtime scene and load from the same data
		m_runtime = std::make_unique<Scene>();
		NE::Serialization::JsonSceneSerializer::DeserializeFromMemory(*m_runtime, m_editorBackup);
		m_runtime->Init();

		m_isPlaying = true;

		// start scripts on runtime
		m_runtime->ScriptStart();
	}

	void SceneManager::StopPlay() {
		if (!m_isPlaying) return;

		// stop scripts first
		if (m_runtime) {
			m_runtime->ScriptStop();
			m_runtime->Exit();
		}

		// destroy runtime scene
		m_runtime.reset();

		m_isPlaying = false;
		// restore editor scene from backup
		if (m_editor && !m_editorBackup.empty()) {
			m_editor->Exit();
			m_editor = std::make_unique<Scene>();
			NE::Serialization::JsonSceneSerializer::DeserializeFromMemory(*m_editor, m_editorBackup);
			m_editor->Init();
		}

	}

	bool SceneManager::IsPlaying() const {
		return m_isPlaying;
	}

	Scene* SceneManager::GetActive() {
		return m_isPlaying ? m_runtime.get() : m_editor.get();
	}

	void SceneManager::Update(double dt) {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Update(dt);
		} else {
			if (m_editor) m_editor->Update(dt);
		}
	}

	void SceneManager::Render(NE::SceneManagement::RenderPass pass) {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Render(pass);
		} else {
			if (m_editor) m_editor->Render(pass);
		}
	}

	void SceneManager::ExitScene() {
		if (m_isPlaying && m_runtime) m_runtime->Exit();
		if (m_editor) m_editor->Exit();
	}

}
