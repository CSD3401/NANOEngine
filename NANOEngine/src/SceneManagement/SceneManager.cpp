#include "SceneManager.hpp"

#include "Scripting/ScriptingEngine.hpp"
#include "ECS/Components/NativeScript.hpp"
#include "ECS/Core/Entity.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "Serialisation/Serializer.hpp"

namespace hack { bool sceneRdy = false; }

namespace NE::SceneManagement {

	void SceneManager::LoadScene(const std::string& path) {
		m_loadedPath = path;
		m_editor = std::make_unique<Scene>();
		NE::Deserialization::DeserializeScene(m_editor->GetECSCoordinator(), path);
		m_editor->Init();
		hack::sceneRdy = true;
		Prefab::PrefabManager::Init(m_editor.get());
		Prefab::PrefabManager::RebuildFromScene();
		m_isPlaying = false;
		m_runtime.reset();
	}

	void SceneManager::LoadRuntime() {
		if (!m_editor || m_isPlaying) return;

		auto& entities = m_editor->GetECSCoordinator().GetComponentManager().GetEntitiesWithComponent<ECS::Component::NativeScript>();
		for (NE::ECS::Entity entity : entities) {
			auto& nsc = m_editor->GetECSCoordinator().GetComponentManager().GetComponent<ECS::Component::NativeScript>(entity);
			if (nsc.Instance) {
				Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);
			}
		}
		Scripting::ScriptingEngine::GetInstance().DestroyAllScriptInstances();

		hack::sceneRdy = false;

		m_runtime = std::make_unique<Scene>();
		NE::Deserialization::DeserializeScene(m_runtime->GetECSCoordinator(), m_loadedPath);
		m_runtime->Init();

		hack::sceneRdy = true;

		m_isPlaying = true;

		m_runtime->ScriptStart();
	}

	void SceneManager::StopRuntime() {
		if (!m_isPlaying) return;

		if (m_runtime) {
			m_runtime->ScriptStop();
			m_runtime->Exit();
			hack::sceneRdy = false;
		}

		m_runtime.reset();

		m_isPlaying = false;
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
