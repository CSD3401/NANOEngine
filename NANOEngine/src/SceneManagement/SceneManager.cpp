#include "SceneManager.hpp"

#include "Scripting/ScriptingEngine.hpp"
#include "Core/SpdLogger.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "Serialisation/Serializer.hpp"

namespace NE::SceneManagement {

	bool SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		Scripting::ScriptingEngine::GetInstance().BeginSceneLoad();
		if (!NE::Deserialization::DeserializeScene(m_editor->GetECSCoordinator(), path)) {
			m_editor.reset();
			Scripting::ScriptingEngine::GetInstance().EndSceneLoad();
			return false;
		}
		m_editor->InitEdit();
		Scripting::ScriptingEngine::GetInstance().EndSceneLoad();
		Prefab::PrefabManager::Init(m_editor.get());
		//Prefab::PrefabManager::RebuildFromScene();
		m_isPlaying = false;
		m_runtime.reset();
		return true;
	}

	void SceneManager::CreateSceneFallback(const std::string& scenePath) {
		m_loadedPath = scenePath;
		m_editor = std::make_unique<Scene>();
		Scripting::ScriptingEngine::GetInstance().BeginSceneLoad();
	}

	void SceneManager::StartSceneFallback() {
		m_editor->InitEdit();
		Scripting::ScriptingEngine::GetInstance().EndSceneLoad();
		Prefab::PrefabManager::Init(m_editor.get());
		//Prefab::PrefabManager::RebuildFromScene();
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
		m_runtime->InitRuntime();
		Scripting::ScriptingEngine::GetInstance().EndSceneLoad();

		m_isPlaying = true;

		// Start scripts in runtime scene
		m_runtime->ScriptStart();
	}

	void SceneManager::StopRuntime() {
		if (!m_isPlaying) return;

		if (m_runtime) {
			m_runtime->ScriptStop();
			m_runtime->ExitRuntime();
		}

		m_runtime.reset();
		m_isPlaying = false;

		// Recreate editor scene script instances for inspection
		// (disabled, no Update calls, but allow field editing in inspector)
		if (m_editor) {
			auto& coordinator = m_editor->GetECSCoordinator();
			auto& componentMgr = coordinator.GetComponentManager();
			auto& entityMgr = coordinator.GetEntityManager();
			auto& luidRegistry = coordinator.GetLUIDRegistry();
			Scripting::ScriptingEngine::GetInstance().RecreateScriptInstances(componentMgr, entityMgr, luidRegistry);
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

	bool SceneManager::LoadPrefabScene(const std::string& path) {
		if (m_prefabScene) {
			m_prefabScene->ExitEdit();
			m_prefabScene.reset();
		}

		m_prefabScene = std::make_unique<Scene>();

		if (NE::Deserialization::DeserializePrefab(m_prefabScene->GetECSCoordinator(), path) == UINT32_MAX) {
			m_prefabScene->ExitEdit();
			m_prefabScene.reset();
			SPD_WARNING("Failed to load prefab, try reimporting");
			return false;
		}
		m_prefabScene->InitEdit();

		m_prefabPath = path;
		m_isEditingPrefab = true;
		return true;
	}

	void SceneManager::ClosePrefabScene() {
		if (m_prefabScene) {
			m_prefabScene->ExitEdit();
			m_prefabScene.reset();
		}
		m_prefabPath.clear();
		m_isEditingPrefab = false;
	}

	void SceneManager::ExitScene() {
		if (m_isPlaying && m_runtime) m_runtime->ExitRuntime();
		if (m_editor) m_editor->ExitEdit();
	}

}
