#include "ScriptSystem.hpp"
#include <iostream>
#include <filesystem>
#include "../Components/NativeScript.hpp"
#include "../../Scripting/IScript.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/Transform.hpp"
#include "Core/SpdLogger.hpp"
#include "Core/Couroutine.hpp"
#include "Events/EventBus.hpp"
#include "../../Scripting/ScriptingEngine.hpp"

namespace hack { extern bool sceneRdy; }

namespace NE::ECS::Systems {
	ScriptSystem::ScriptSystem(ComponentManager* cm, EntityManager* em)
		: m_componentManager(cm), m_entityManager(em) {

		// Set ECS references in ScriptEngine so it can manage instances
		Scripting::ScriptingEngine::GetInstance().SetECSReferences(cm, em);
	}

	void ScriptSystem::OnEntityAdded(Entity entity) {
		// Only manage component data, delegate instance creation to ScriptEngine
		if (!hack::sceneRdy) {
			return;
		}

		auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

		// Check if script name is valid
		if (nsc.ScriptName.empty()) {
			SPD_WARNING("NativeScript component on entity " << (int)entity << " has empty ScriptName");
			return;
		}

		// Create script instance via ScriptEngine
		if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstance(entity, nsc)) {
			// Initialize the script instance
			Scripting::ScriptingEngine::GetInstance().InitializeScriptInstance(entity);

			// Clear dirty flag after successful creation
			nsc.IsDirty = false;
		}
	}

	void ScriptSystem::OnEntityRemoved(Entity entity) {
		// Delegate instance cleanup to ScriptEngine
		if (!m_componentManager->HasComponent<Component::NativeScript>(entity)) {
			return;
		}

		// Destroy script instance via ScriptEngine
		Scripting::ScriptingEngine::GetInstance().DestroyScriptInstance(entity);
	}

	void ScriptSystem::Init() {
		SPD_INFO("ScriptSystem::Init() - Starting initialization");

		// Initialize existing scripts in the scene
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			// Skip if script name is empty
			if (nsc.ScriptName.empty()) {
				continue;
			}

			// Create script instance via ScriptEngine
			if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstance(entity, nsc)) {
				// Initialize the script instance
				Scripting::ScriptingEngine::GetInstance().InitializeScriptInstance(entity);

				// Clear dirty flag after successful creation
				nsc.IsDirty = false;

				SPD_INFO("Initialized existing script '" << nsc.ScriptName
					<< "' for entity " << (int)entity << " during ScriptSystem::Init()");
			}
			else {
				SPD_WARNING("Failed to initialize script '" << nsc.ScriptName
					<< "' for entity " << (int)entity);
			}
		}

		SPD_INFO("ScriptSystem::Init() - Completed");
	}

	void ScriptSystem::Exit() {
		SPD_INFO("ScriptSystem::Exit() - Starting cleanup");

		// CRITICAL: Clear these FIRST before ANY cleanup
		// Prevents dangling function pointers to script code
		NANOEngine::Events::ClearScriptEventListeners();
		Engine_ClearAllCoroutines();

		auto& entities = GetEntities();

		for (NE::ECS::Entity entity : entities) {
			if (!m_componentManager->HasComponent<Component::NativeScript>(entity)) {
				continue;
			}

			// Destroy script instance via ScriptEngine
			Scripting::ScriptingEngine::GetInstance().DestroyScriptInstance(entity);
		}

		SPD_INFO("ScriptSystem::Exit() - Completed");
	}

	void ScriptSystem::StartScripts()
	{
		SPD_INFO("ScriptSystem: Entering Play Mode...");

		// Delegate to ScriptEngine
		Scripting::ScriptingEngine::GetInstance().StartAllScriptInstances();
	}

	void ScriptSystem::PauseScripts()
	{
		SPD_INFO("ScriptSystem: Pausing Play Mode...");

		// Delegate to ScriptEngine
		Scripting::ScriptingEngine::GetInstance().PauseAllScriptInstances();
	}

	void ScriptSystem::StopScripts()
	{
		SPD_INFO("ScriptSystem: Exiting Play Mode...");

		// Delegate to ScriptEngine
		Scripting::ScriptingEngine::GetInstance().StopAllScriptInstances();
	}

	void ScriptSystem::Update(double deltaTime) {
		// --- Check for compile request ---
		if (Scripting::ScriptingEngine::GetInstance().m_compileQueued.load()) {
			Scripting::ScriptingEngine::GetInstance().m_compileQueued.store(false);
			Scripting::ScriptingEngine::GetInstance().HotCompileAndReload();
		}

		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		// First pass: Check for dirty components and recreate instances if needed
		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			// If component is dirty, recreate the script instance
			if (nsc.IsDirty) {
				SPD_INFO("NativeScript component for entity " << (int)entity
					<< " is dirty, recreating script instance");

				// Destroy old instance
				Scripting::ScriptingEngine::GetInstance().DestroyScriptInstance(entity);

				// Create new instance if script name is not empty
				if (!nsc.ScriptName.empty()) {
					if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstance(entity, nsc)) {
						Scripting::ScriptingEngine::GetInstance().InitializeScriptInstance(entity);
					}
				}

				// Clear dirty flag
				nsc.IsDirty = false;
			}
		}

		// Second pass: Delegate script updates to ScriptEngine
		// ScriptEngine will handle all the runtime logic (Start, Update, hierarchy checks, etc.)
		Scripting::ScriptingEngine::GetInstance().UpdateScriptInstances(deltaTime);
	}
}
