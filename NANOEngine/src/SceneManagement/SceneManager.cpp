#include "SceneManager.hpp"
#include "Serialisation/JsonSceneSerializer.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include <Core/SpdLogger.hpp>  // For SPD_INFO logging
#include "PrefabManagement/PrefabManager.hpp"

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		NE::Serialization::JsonSceneSerializer::Deserialize(*m_editor, path);
		m_editor->Init();
		Prefab::PrefabManager::Init(m_editor.get());
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

	void SceneManager::SaveSceneIfDirty(const std::string& path) {
		// Only save in Edit mode
		if (m_isPlaying) return;
		
		if (!m_editor || !m_editor->IsDirty()) return;

		std::string savePath = path.empty() ? m_loadedPath : path;
		if (savePath.empty()) return;

		SPD_INFO("[DirtyFlag] Saving scene to: {}", savePath);
		Serialization::JsonSceneSerializer::Serialize(*m_editor, savePath);
		m_editor->ClearDirty();
		SPD_INFO("[DirtyFlag] Scene saved and marked as CLEAN");
	}

	void SceneManager::BeginPlay() {
		if (!m_editor || m_isPlaying) return;

		// This ensures component references and other script fields are preserved
		auto& entities = m_editor->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
		for (NE::ECS::Entity entity : entities) {
			auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
			if (nsc.Instance) {
				Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);
			}
		}

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
			// CRITICAL: Null out editor script instances before Exit()
			// During hot reload, only runtime scene was updated. Editor scene scripts
			// still reference the OLD unloaded DLL, causing crashes in Exit().
			// We null them out so Exit() doesn't try to call methods on stale DLL code.
			auto& entities = m_editor->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
			for (NE::ECS::Entity entity : entities) {
				auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
				if (nsc.Instance) {
					// Don't call any methods on Instance - it may reference freed DLL!
					// Just manually delete and null it out
					delete nsc.Instance;
					nsc.Instance = nullptr;
				}
				// Clear function pointers too (they also reference old DLL)
				nsc.CreateScript = nullptr;
				nsc.DestroyScript = nullptr;
			}

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
		if (m_isPlaying)          return m_runtime.get();
		if (m_isEditingPrefab)    return m_prefabScene.get();
		return m_editor.get();
	}

	void SceneManager::Update(double dt) {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Update(dt);
		} else if (m_isEditingPrefab) {
			if (m_prefabScene) m_prefabScene->Update(dt);
		} else {
			if (m_editor) m_editor->Update(dt);
		}
	}

	void SceneManager::Render(NE::SceneManagement::RenderPass pass) {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Render(pass);
		} else if (m_isEditingPrefab) {
			if (m_prefabScene) m_prefabScene->Render(pass);
		} else {
			if (m_editor) m_editor->Render(pass);
		}
	}

	void SceneManager::LoadPrefabScene(const std::string& path) {
		if (m_prefabScene) {
			m_prefabScene->Exit();
			m_prefabScene.reset();
		}

		m_prefabScene = std::make_unique<Scene>();

		NE::Serialization::JsonSceneSerializer::Deserialize(*m_prefabScene, path);
		m_prefabScene->Init();

		m_prefabPath = path;
		m_isEditingPrefab = true;
	}

	void SceneManager::ClosePrefabScene() {
		if (m_prefabScene) {
			m_prefabScene->Exit();
			m_prefabScene.reset();
		}
		m_prefabPath.clear();
		m_isEditingPrefab = false;
	}

	void SceneManager::ExitScene() {
		if (m_isPlaying && m_runtime) m_runtime->Exit();
		if (m_editor) m_editor->Exit();
	}

}
