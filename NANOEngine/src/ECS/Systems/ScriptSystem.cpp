#include "ScriptSystem.hpp" 
#include <iostream>
#include <filesystem>
#include "../Components/NativeScript.hpp"
#include "Core/SpdLogger.hpp"
#include "Scripting/ScriptingEngine.hpp"

// Entire scripting requires a major refactor ~ irwen

namespace NE::ECS::Systems {

    // --- Helper function to get the temporary DLL path ---
    ScriptSystem::ScriptSystem(ComponentManager* cm)
        : m_componentManager(cm) {
    }

    //ScriptSystem::~ScriptSystem() {
    //    m_sourceWatcher.reset();

    //    // --- Clean up the last loaded DLL ---
    //    // Ensure we don't leave temp files.
    //    try {
    //        if (!m_currentLoadedDLLPath.empty() && std::filesystem::exists(m_currentLoadedDLLPath)) {
    //            std::filesystem::path dllPath(m_currentLoadedDLLPath);
    //            std::filesystem::path pdbPath = dllPath.replace_extension(".pdb");

    //            std::filesystem::remove(dllPath);
    //            if (std::filesystem::exists(pdbPath)) {
    //                std::filesystem::remove(pdbPath);
    //            }
    //        }

    //        SPD_INFO("Constructor deletes.");
    //    }
    //    catch (const std::exception& e) {
    //        SPD_WARNING("Could not clean up temp DLL: " << e.what());
    //    }
    //}

    void ScriptSystem::OnEntityAdded(Entity entity) {
        // Logic for when an entity relevant to the script system is added
        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
        if (nsc.CreateScript && !nsc.Instance) {
            nsc.Instance = nsc.CreateScript();
            nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
            nsc.Instance->SetEntity(entity);
      
            // Call Awake() first (even if disabled)
            nsc.Instance->Awake();
    
            // Then Initialize()
            nsc.Instance->Initialize(entity);
            
            // Restore serialized field values if they exist
            Scripting::ScriptingEngine::GetInstance().RestoreSerializedFields(nsc);

            nsc.Instance->SetEnabled(false); // Start disabled
            SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

    void ScriptSystem::OnEntityRemoved(Entity entity) {
        // should probably call OnScriptComponentDestroyed here if the entity
        // has a script component, to ensure proper cleanup.
        (void)entity;
    }

    void ScriptSystem::Init() {
        SPD_INFO("ScriptSystem::Init() - Starting initialization");
        
        // Logic to initialize the system when a scene loads

        InitializeExistingScripts();

        SPD_INFO("ScriptSystem::Init() - Completed");
    }

    void ScriptSystem::Exit() {
        auto& entities = GetEntities();

        for (NE::ECS::Entity entity : entities) {
            Scripting::ScriptingEngine::GetInstance().OnScriptComponentDestroyed(entity);
            auto& ns = m_componentManager->GetComponent<Component::NativeScript>(entity);
            ns.CreateScript = {};   // or ns.CreateScript = nullptr; if it’s a function ptr
            ns.DestroyScript = {};
        }
    }

    // WOI WENGKONG IDK IF THIS IS HOW ITS SUPPOSED TO BE DONE HELP ME CHECK BUT IT WORKS FOR NOW
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
                nsc.Instance->LinkToEngine(m_componentManager);
                nsc.Instance->SetEntity(entity);

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
                nsc.Instance->RefreshComponentReferences();
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
                // Save field values before destroying
                Scripting::ScriptingEngine::GetInstance().SaveSerializedFields(nsc);
  
                nsc.Instance->OnDestroy();
                if (nsc.DestroyScript) {
                    nsc.DestroyScript(nsc.Instance);
                }
                else {
                    delete nsc.Instance; // Fallback
                }
                // CRITICAL: Reset the instance pointer to null
                nsc.Instance = nullptr;
                SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
            }

            // Recreate the script instance for the next play session
            if (nsc.CreateScript && !nsc.Instance) {
                nsc.Instance = nsc.CreateScript();
                nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
                nsc.Instance->SetEntity(entity);
        
                // Call Awake() and Initialize()
                nsc.Instance->Awake();
                nsc.Instance->Initialize(entity);
  
                // Restore the saved field values
                Scripting::ScriptingEngine::GetInstance().RestoreSerializedFields(nsc);
            
	            nsc.Instance->SetEnabled(false); // Start disabled
                SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
            }
        }
    }
    

    void ScriptSystem::Update(double deltaTime) {

        // --- Check for compile request ---
        if (Scripting::ScriptingEngine::GetInstance().m_compileQueued.load()) {
            Scripting::ScriptingEngine::GetInstance().m_compileQueued.store(false); // Consume the flag
            Scripting::ScriptingEngine::GetInstance().HotCompileAndReload(); // Run the compile and reload
        }

        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        // Second loop: Update all active scripts
        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
            if (nsc.Instance && nsc.Instance->IsEnabled()) {
                // Call Start() before first Update() if not yet called
                if (!nsc.Instance->HasStarted()) {
                    nsc.Instance->Start();
                    nsc.Instance->MarkStartCalled();
                }
        
                nsc.Instance->Update(deltaTime);
            }
        }
    }
}