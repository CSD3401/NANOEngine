#include "SceneManager.hpp"
#include "Serialisation/JsonSceneSerializer.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include <Core/SpdLogger.hpp>  // For SPD_INFO logging
#include "PrefabManagement/PrefabManager.hpp"
#include "Graphics/Core/GraphicsManager.hpp"

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		NE::Serialization::JsonSceneSerializer::Deserialize(*m_editor, path);
		m_editor->Init();
		Prefab::PrefabManager::Init(m_editor.get());
		Prefab::PrefabManager::RebuildFromScene();
		m_isPlaying = false;
		m_runtime.reset();

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
		Graphics::GraphicsManager::m_lights.clear();
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
		Graphics::GraphicsManager::m_lights.clear();
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
			Prefab::PrefabManager::Init(m_editor.get());
			Prefab::PrefabManager::RebuildFromScene();
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
