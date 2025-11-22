#include "SceneManager.hpp"
#include "Serialisation/JsonSceneSerializer.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include <Core/SpdLogger.hpp>  // For SPD_INFO logging

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

	void SceneManager::SaveSceneIfDirty(const std::string& path) {
		// Only save in Edit mode
		if (m_isPlaying) return;
		
		if (!m_editor || !m_editor->IsDirty()) return;

		std::string savePath = path.empty() ? m_loadedPath : path;
		if (savePath.empty()) return;

		SPD_INFO("[DirtyFlag] Saving scene to: {}", savePath);

		// CRITICAL: Capture current field values from script instances before serializing
		// This ensures any changes made in the editor inspector are persisted
		auto& entities = m_editor->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
		for (NE::ECS::Entity entity : entities) {
			auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
			if (nsc.Instance) {
				Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);
			}
		}

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

		// 2) Destroy editor scene script instances to prevent memory leaks
		// We don't need them during play mode - only runtime scene needs active scripts
		for (NE::ECS::Entity entity : entities) {
			auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
			if (nsc.Instance) {
				// Properly destroy the instance
				Scripting::ScriptingEngine::GetInstance().OnScriptComponentDestroyed(entity);
			}
			// Clear function pointers
			nsc.CreateScript = nullptr;
			nsc.DestroyScript = nullptr;
		}

		// 3) create runtime scene and load from the same data
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
			// NOTE: Editor scene script instances were already destroyed in BeginPlay()
			// This loop is a safety check in case any instances were somehow created during play
			// (they shouldn't be, since editor scene isn't updated during play mode)
			auto& entities = m_editor->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
			for (NE::ECS::Entity entity : entities) {
				auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
				if (nsc.Instance) {
					// Safety: null out any unexpected instances
					// Don't delete - may reference old DLL if hot reload occurred
					nsc.Instance = nullptr;
				}
				// Clear function pointers
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
		return m_isPlaying ? m_runtime.get() : m_editor.get();
	}

	void SceneManager::Update(double dt) {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Update(dt);
		} else {
			if (m_editor) m_editor->Update(dt);
		}
	}

	void SceneManager::Render() {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Render();
		} else {
			if (m_editor) m_editor->Render();
		}
	}

	void SceneManager::ExitScene() {
		if (m_isPlaying && m_runtime) m_runtime->Exit();
		if (m_editor) m_editor->Exit();
	}

}
