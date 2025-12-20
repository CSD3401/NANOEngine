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

		// Step 1: Save all editor script field values to their components
		auto& editorEntities = m_editor->GetECSCoordinator().GetComponentManager()
			.GetEntitiesWithComponent<ECS::Component::NativeScript>();

		// Build map of LUID -> SerializedFields from editor scene
		std::unordered_map<uint64_t, std::unordered_map<std::string, std::string>> editorFieldsByLUID;
		std::unordered_map<uint64_t, std::unordered_set<std::string>> editorRefFieldsByLUID;

		for (NE::ECS::Entity entity : editorEntities) {
			auto& nsc = m_editor->GetECSCoordinator().GetComponentManager()
				.GetComponent<ECS::Component::NativeScript>(entity);

			// Save current instance field values to component
			Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);

			// Store in LUID map for transfer to runtime
			editorFieldsByLUID[nsc.luid] = nsc.SerializedFields;
			editorRefFieldsByLUID[nsc.luid] = nsc.EntityReferenceFields;
		}

		// Step 2: Destroy all script instances from editor scene
		Scripting::ScriptingEngine::GetInstance().DestroyAllScriptInstances();

		hack::sceneRdy = false;

		// Step 3: Load runtime scene from file
		m_runtime = std::make_unique<Scene>();
		NE::Deserialization::DeserializeScene(m_runtime->GetECSCoordinator(), m_loadedPath);

		// Step 4: Transfer editor field values to runtime components (before Init)
		auto& runtimeEntities = m_runtime->GetECSCoordinator().GetComponentManager()
			.GetEntitiesWithComponent<ECS::Component::NativeScript>();

		for (NE::ECS::Entity entity : runtimeEntities) {
			auto& runtimeNsc = m_runtime->GetECSCoordinator().GetComponentManager()
				.GetComponent<ECS::Component::NativeScript>(entity);

			// Find matching editor component by LUID
			auto fieldsIt = editorFieldsByLUID.find(runtimeNsc.luid);
			if (fieldsIt != editorFieldsByLUID.end()) {
				// Copy editor's field values to runtime component
				runtimeNsc.SerializedFields = fieldsIt->second;
			}

			auto refFieldsIt = editorRefFieldsByLUID.find(runtimeNsc.luid);
			if (refFieldsIt != editorRefFieldsByLUID.end()) {
				// Copy entity reference field markers
				runtimeNsc.EntityReferenceFields = refFieldsIt->second;
			}
		}

		// Step 5: Initialize runtime scene (will create instances with transferred field values)
		m_runtime->Init();

		hack::sceneRdy = true;

		m_isPlaying = true;

		// Step 6: Start scripts in runtime scene
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

		// Recreate editor scene script instances for inspection
		// These will be disabled (no Update calls) but allow field editing in inspector
		if (m_editor) {
			// CRITICAL: Reset ECS references to editor scene's managers
			// (they were pointing to the now-destroyed runtime scene)
			Scripting::ScriptingEngine::GetInstance().SetECSReferences(
				&m_editor->GetECSCoordinator().GetComponentManager(),
				&m_editor->GetECSCoordinator().GetEntityManager()
			);

			auto& entities = m_editor->GetECSCoordinator().GetComponentManager()
				.GetEntitiesWithComponent<ECS::Component::NativeScript>();

			for (NE::ECS::Entity entity : entities) {
				auto& nsc = m_editor->GetECSCoordinator().GetComponentManager()
					.GetComponent<ECS::Component::NativeScript>(entity);

				// Skip empty script names
				if (nsc.ScriptName.empty()) continue;

				// Create instance (ScriptEngine will initialize and restore serialized fields)
				if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstance(entity, nsc)) {
					Scripting::ScriptingEngine::GetInstance().InitializeScriptInstance(entity);
				}
			}
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
