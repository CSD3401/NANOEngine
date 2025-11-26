#include "ScriptSystem.hpp"
#include <iostream>
#include <filesystem>
#include "../Components/NativeScript.hpp"
#include "../../Scripting/IScript.hpp"  // Explicitly include IScript definition
#include "../Components/EntityMeta.hpp"
#include "../Components/Transform.hpp"  // CRITICAL: Add Transform header for hierarchy checking
#include "Core/SpdLogger.hpp"
#include "Core/Couroutine.hpp"
#include "Events/EventBus.hpp"
#include "../../Scripting/ScriptingEngine.hpp"
#include "../../Scripting/ScriptContextFactory.hpp"

namespace hack { extern bool sceneRdy; }

namespace NE::ECS::Systems {
	ScriptSystem::ScriptSystem(ComponentManager* cm, EntityManager* em)
		: m_componentManager(cm), m_entityManager(em) {
	}

	void ScriptSystem::OnEntityAdded(Entity entity) {

		if (hack::sceneRdy) {
			// Logic for when an entity relevant to the script system is added
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
			if (nsc.CreateScript && !nsc.Instance) {
				nsc.Instance = nsc.CreateScript();
				Scripting::LinkScriptToEngine(nsc.Instance, m_componentManager, m_entityManager); // Link to engine systems via new API
				nsc.Instance->_SetEntity(entity);

				// Call Awake() first (even if disabled)
				nsc.Instance->Awake();

				// Then Initialize()
				nsc.Instance->Initialize(entity);

				// Restore serialized field values if they exist
				Scripting::ScriptingEngine::GetInstance().RestoreSerializedFields(nsc);

				nsc.Instance->SetEnabled(false); // Start disabled
				SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
			}
			else {
				auto factory = Scripting::ScriptingEngine::GetInstance().GetScriptFactory(nsc.ScriptName);

				if (factory) {
					if (nsc.Instance && nsc.DestroyScript) {
						nsc.DestroyScript(nsc.Instance);
					}
					else if (nsc.Instance) {
						delete nsc.Instance;
					}

					nsc.CreateScript = factory;
					nsc.DestroyScript = [](IScript* instance) { delete instance; };
					nsc.Instance = nsc.CreateScript();
					Scripting::LinkScriptToEngine(nsc.Instance, m_componentManager, m_entityManager);
					nsc.Instance->_SetEntity(entity);

					nsc.Instance->Awake();

					nsc.Instance->Initialize(entity);

					Scripting::ScriptingEngine::GetInstance().RestoreSerializedFields(nsc);

					nsc.Instance->SetEnabled(false);
					SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
				}
			}
		}

		
	}

    void ScriptSystem::OnEntityRemoved(Entity entity) {
        // Clean up script instance when entity is removed
        if (!m_componentManager->HasComponent<Component::NativeScript>(entity)) {
            return;
        }

        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

        // Destroy script instance if it exists
        if (nsc.Instance) {
            // Call OnDestroy and properly clean up
            Scripting::ScriptingEngine::GetInstance().OnScriptComponentDestroyed(entity);
        }

        // Clear function pointers to prevent stale DLL references
        nsc.CreateScript = nullptr;
        nsc.DestroyScript = nullptr;
    }
    
	void ScriptSystem::Init() {
		SPD_INFO("ScriptSystem::Init() - Starting initialization");

		// Logic to initialize the system when a scene loads

		InitializeExistingScripts();

		SPD_INFO("ScriptSystem::Init() - Completed");
	}

	void ScriptSystem::Exit() {
		// CRITICAL: Clear these FIRST before ANY cleanup
		// Prevents dangling function pointers to script code
		NANOEngine::Events::ClearScriptEventListeners();
		Engine_ClearAllCoroutines();

		auto& entities = GetEntities();

		for (NE::ECS::Entity entity : entities) {
			if (!m_componentManager->HasComponent<Component::NativeScript>(entity)) {
				continue;
			}

			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			// Destroy script instances
			if (nsc.Instance) {
				// Call OnDestroy and properly clean up
				Scripting::ScriptingEngine::GetInstance().OnScriptComponentDestroyed(entity);
			}

			// Clear function pointers to prevent stale DLL references
			nsc.CreateScript = nullptr;
			nsc.DestroyScript = nullptr;
		}
	}

	void ScriptSystem::InitializeExistingScripts() {
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (!nsc.ScriptName.empty()) {
				nsc.CreateScript = Scripting::ScriptingEngine::GetInstance().GetScriptFactory(nsc.ScriptName);
				nsc.DestroyScript = [](IScript* s) { delete s; };
			}

			if (nsc.CreateScript && !nsc.Instance) {
				nsc.Instance = nsc.CreateScript();
				Scripting::LinkScriptToEngine(nsc.Instance, m_componentManager, m_entityManager);
				nsc.Instance->_SetEntity(entity);

				nsc.Instance->Awake();
				nsc.Instance->Initialize(entity);

				Scripting::ScriptingEngine::GetInstance().RestoreSerializedFields(nsc);

				nsc.Instance->SetEnabled(false);

				SPD_INFO("Initialized existing script '" << nsc.ScriptName
					<< "' for entity " << (int)entity << " during ScriptSystem::Init()");
			}
		}
	}

	void ScriptSystem::StartScripts()
	{
		SPD_INFO("ScriptSystem: Entering Play Mode...");
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance) {
				nsc.Instance->_RefreshComponentReferences();
				nsc.Instance->SetEnabled(true);
			}
		}
	}

	void ScriptSystem::PauseScripts()
	{
		SPD_INFO("ScriptSystem: Pausing Play Mode...");
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance) {
				nsc.Instance->SetEnabled(false);
				SPD_INFO("Paused script '" << nsc.ScriptName << "' for entity " << (int)entity);
			}
		}
	}

	void ScriptSystem::StopScripts()
	{
		SPD_INFO("ScriptSystem: Exiting Play Mode...");
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance) {
				// Disable the script
				nsc.Instance->SetEnabled(false);

				// Save field values for potential restoration
				Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);

				SPD_INFO("Stopped script '" << nsc.ScriptName << "' for entity " << (int)entity);
			}
		}

		// Note: Instances are NOT destroyed here.
		// The runtime scene will be destroyed via Exit(), which properly cleans up instances.
		// For editor scene, scripts were never started, so just disabling is fine.
	}

	void ScriptSystem::Update(double deltaTime) {
		// --- Check for compile request ---
		if (Scripting::ScriptingEngine::GetInstance().m_compileQueued.load()) {
			Scripting::ScriptingEngine::GetInstance().m_compileQueued.store(false); // Consume the flag
			Scripting::ScriptingEngine::GetInstance().HotCompileAndReload(); // Run the compile and reload
		}

		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		// Update all active scripts
		for (Entity entity : entities) {
			// CRITICAL FIX: Check if entity is active IN HIERARCHY (not just own isActive!)
			  // This prevents scripts from running when parent is disabled (Unity-style)
			bool shouldUpdate = true;

			if (m_componentManager->HasComponent<Component::EntityMeta>(entity)) {
				const auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(entity);

				// First check: entity's own isActive flag
				if (!meta.isActive) {
					shouldUpdate = false;
				}
				// Second check: walk up parent chain to see if any parent is disabled
				else if (m_componentManager->HasComponent<Component::Transform>(entity)) {
					const auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
					Entity currentParent = transform.parent;

					while (currentParent != Component::INVALID_PARENT) {
						if (m_componentManager->HasComponent<Component::EntityMeta>(currentParent)) {
							const auto& parentMeta = m_componentManager->GetComponent<Component::EntityMeta>(currentParent);
							if (!parentMeta.isActive) {
								shouldUpdate = false;
								break; // Parent is disabled, so we're inactive in hierarchy
							}
						}

						// Move up to next parent
						if (m_componentManager->HasComponent<Component::Transform>(currentParent)) {
							const auto& parentTransform = m_componentManager->GetComponent<Component::Transform>(currentParent);
							currentParent = parentTransform.parent;
						}
						else {
							break;
						}
					}
				}
			}

			// Skip this entity if inactive in hierarchy
			if (!shouldUpdate) {
				continue;
			}

			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
			if (nsc.Instance && nsc.Instance->IsEnabled()) {
				// Call Start() before first Update() if not yet called
				if (!nsc.Instance->_HasStarted()) {
					nsc.Instance->Start();
					nsc.Instance->_MarkStartCalled();
				}

				nsc.Instance->Update(deltaTime);
			}
		}
	}
}