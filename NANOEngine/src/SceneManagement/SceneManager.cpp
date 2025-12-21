#include "SceneManager.hpp"

#include "Scripting/ScriptingEngine.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "Serialisation/Serializer.hpp"

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		Scripting::ScriptingEngine::GetInstance().BeginSceneLoad();
		NE::Deserialization::DeserializeScene(m_editor->GetECSCoordinator(), path);
		m_editor->Init();
		Scripting::ScriptingEngine::GetInstance().EndSceneLoad();
		Prefab::PrefabManager::Init(m_editor.get());
		Prefab::PrefabManager::RebuildFromScene();
		m_isPlaying = false;
		m_runtime.reset();
	}

	void SceneManager::LoadRuntime() {
		if (!m_editor || m_isPlaying) return;

		// Save editor script field values and transfer to runtime
		auto& editorComponentMgr = m_editor->GetECSCoordinator().GetComponentManager();
		Scripting::ScriptingEngine::GetInstance().SaveSceneScriptFields(editorComponentMgr);

		// Destroy all editor script instances
		Scripting::ScriptingEngine::GetInstance().DestroyAllScriptInstances();

		// Load runtime scene from file
		m_runtime = std::make_unique<Scene>();
		Scripting::ScriptingEngine::GetInstance().BeginSceneLoad();
		NE::Deserialization::DeserializeScene(m_runtime->GetECSCoordinator(), m_loadedPath);

		// Transfer editor field values to runtime scene (before Init)
		auto& runtimeComponentMgr = m_runtime->GetECSCoordinator().GetComponentManager();
		Scripting::ScriptingEngine::GetInstance().TransferScriptFields(editorComponentMgr, runtimeComponentMgr);

		// Initialize runtime scene (creates instances with transferred field values)
		m_runtime->Init();
		Scripting::ScriptingEngine::GetInstance().EndSceneLoad();

		m_isPlaying = true;

		// Start scripts in runtime scene
		m_runtime->ScriptStart();
	}

	void SceneManager::StopRuntime() {
		if (!m_isPlaying) return;

		if (m_runtime) {
			m_runtime->ScriptStop();
			m_runtime->Exit();
		}

		m_runtime.reset();
		m_isPlaying = false;

		// Recreate editor scene script instances for inspection
		// (disabled, no Update calls, but allow field editing in inspector)
		if (m_editor) {
			auto& componentMgr = m_editor->GetECSCoordinator().GetComponentManager();
			auto& entityMgr = m_editor->GetECSCoordinator().GetEntityManager();
			Scripting::ScriptingEngine::GetInstance().RecreateScriptInstances(componentMgr, entityMgr);
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
			if (m_runtime) m_runtime->UpdateRuntime(dt);
		} else if (m_isEditingPrefab) {
			if (m_prefabScene) m_prefabScene->UpdateEdit(dt);
		} else {
			if (m_editor) m_editor->UpdateEdit(dt);
		}
	}

	void SceneManager::Render() {
		if (m_isPlaying) {
			if (m_runtime) m_runtime->Render();
		} else if (m_isEditingPrefab) {
			if (m_prefabScene) m_prefabScene->Render();
		} else {
			if (m_editor) m_editor->Render();
		}
	}

	void SceneManager::LoadPrefabScene(const std::string& path) {
		if (m_prefabScene) {
			m_prefabScene->Exit();
			m_prefabScene.reset();
		}

		m_prefabScene = std::make_unique<Scene>();

		//NE::Serialization::JsonSceneSerializer::Deserialize(*m_prefabScene, path);
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
