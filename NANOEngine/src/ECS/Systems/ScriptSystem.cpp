#include "ScriptSystem.hpp"
#include <iostream>
#include <filesystem>
#include "../Components/NativeScript.hpp"
#include "../../Scripting/IScript.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/Transform.hpp"
#include "Core/SpdLogger.hpp"
#include "Core/Couroutine.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Events/EventBus.hpp"
#include "../../Scripting/ScriptingEngine.hpp"
#include <algorithm>

namespace NE::ECS::Systems {
	ScriptSystem::ScriptSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
		: m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) {

		// Set ECS references in ScriptEngine so it can manage instances
		Scripting::ScriptingEngine::GetInstance().SetECSReferences(cm, em, lr);
	}

	void ScriptSystem::OnEntityAdded(Entity entity) {
		auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

		// Generate LUID if not set (same as other components like Transform, Renderer, etc.)
		if (nsc.luid == 0) {
			nsc.luid = Core::LUIDGenerator::Generate("ns");
		}

		// Register with LUID registry
		m_luidRegistry->Register(nsc.luid, &nsc, entity);

		// Only manage component data, delegate instance creation to ScriptEngine
		if (!Scripting::ScriptingEngine::GetInstance().ShouldCreateInstancesOnEntityAdded()) {
			return;
		}

		// Check if script names are valid
		if (nsc.ScriptNames.empty()) {
			SPD_WARNING("NativeScript component on entity " << (int)entity << " has empty ScriptNames");
			return;
		}

		// Create script instances via ScriptEngine
		if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstances(entity, nsc)) {
			// Initialize the script instances
			Scripting::ScriptingEngine::GetInstance().InitializeScriptInstances(entity);

			// Update tracking and clear dirty flag after successful creation
			nsc._lastScriptNames = nsc.ScriptNames;
			nsc.IsDirty = false;
		}
	}

	void ScriptSystem::OnEntityRemoved(Entity entity) {
		// Unregister from LUID registry
		if (m_componentManager->HasComponent<Component::NativeScript>(entity)) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
			m_luidRegistry->Unregister(nsc.luid);
		}

		// Destroy script instances via ScriptEngine
		Scripting::ScriptingEngine::GetInstance().DestroyScriptInstances(entity);
	}

	void ScriptSystem::Init() {
		SPD_INFO("ScriptSystem::Init() - Starting initialization");

		// Initialize existing scripts in the scene
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			// Generate LUID if not set (for entities loaded from scene files)
			if (nsc.luid == 0) {
				nsc.luid = Core::LUIDGenerator::Generate("ns");
				m_luidRegistry->Register(nsc.luid, &nsc, entity);
			}

			// Skip if script names are empty
			if (nsc.ScriptNames.empty()) {
				continue;
			}

			// Create script instances via ScriptEngine
			if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstances(entity, nsc)) {
				// Initialize the script instances
				Scripting::ScriptingEngine::GetInstance().InitializeScriptInstances(entity);

				// Update tracking and clear dirty flag after successful creation
				nsc._lastScriptNames = nsc.ScriptNames;
				nsc.IsDirty = false;

				// Build script list string for logging (skip empty strings)
				std::string scriptList;
				for (size_t i = 0; i < nsc.ScriptNames.size(); ++i) {
					if (nsc.ScriptNames[i].empty()) continue;  // Skip empty strings
					if (!scriptList.empty()) scriptList += ", ";
					scriptList += nsc.ScriptNames[i];
				}
				SPD_INFO("Initialized existing scripts [" << scriptList
					<< "] for entity " << (int)entity << " during ScriptSystem::Init()");
			}
			else {
				SPD_WARNING("Failed to initialize scripts for entity " << (int)entity);
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

			// Destroy script instances via ScriptEngine
			Scripting::ScriptingEngine::GetInstance().DestroyScriptInstances(entity);
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

		// First pass: Check for script type changes or initial creation
		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			// Check if script list changed
			bool scriptsChanged = (nsc.ScriptNames != nsc._lastScriptNames);

			if (scriptsChanged) {
				// Build script list strings for logging (skip empty strings)
				std::string oldList, newList;
				for (size_t i = 0; i < nsc._lastScriptNames.size(); ++i) {
					if (nsc._lastScriptNames[i].empty()) continue;
					if (!oldList.empty()) oldList += ", ";
					oldList += nsc._lastScriptNames[i];
				}
				for (size_t i = 0; i < nsc.ScriptNames.size(); ++i) {
					if (nsc.ScriptNames[i].empty()) continue;
					if (!newList.empty()) newList += ", ";
					newList += nsc.ScriptNames[i];
				}

				SPD_INFO("Scripts changed for entity " << (int)entity
					<< " from [" << oldList << "] to [" << newList << "]");

				// Synchronize instances (add new, remove deleted, preserve existing)
				Scripting::ScriptingEngine::GetInstance().SynchronizeScriptInstances(entity, nsc);
			}
			else if (nsc._lastScriptNames.empty() && !nsc.ScriptNames.empty()) {
				// Initial creation (first time this component has scripts assigned)
				// Only triggers when _lastScriptNames is empty (never been initialized)

				std::string scriptList;
				for (size_t i = 0; i < nsc.ScriptNames.size(); ++i) {
					if (nsc.ScriptNames[i].empty()) continue;
					if (!scriptList.empty()) scriptList += ", ";
					scriptList += nsc.ScriptNames[i];
				}
				SPD_INFO("Creating initial script instances for entity " << (int)entity
					<< " scripts [" << scriptList << "]");

				if (Scripting::ScriptingEngine::GetInstance().CreateScriptInstances(entity, nsc)) {
					Scripting::ScriptingEngine::GetInstance().InitializeScriptInstances(entity);
				}

				// Update tracking
				nsc._lastScriptNames = nsc.ScriptNames;
				nsc.IsDirty = false;
			}
			// else: Field edits or other changes - instances already exist with updated values, no action needed
		}

		// Second pass: Delegate script updates to ScriptEngine
		// ScriptEngine will handle all the runtime logic (Start, Update, hierarchy checks, etc.)
		Scripting::ScriptingEngine::GetInstance().UpdateScriptInstances(deltaTime);
	}
}
