#include "ScriptSystem.hpp" 
#include <iostream>
#include "../Components/NativeScript.hpp"

namespace NE::ECS::Systems {

    ScriptSystem::ScriptSystem(ComponentManager* cm)
        : m_componentManager(cm) {
        // Constructor body can be empty if just need to initialize members
    }

    void ScriptSystem::OnEntityAdded(Entity entity) {
        // Logic for when an entity relevant to the script system is added
        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
        if (nsc.CreateScript && !nsc.Instance) {
            nsc.Instance = nsc.CreateScript();
            nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
            nsc.Instance->SetEntity(entity);
            nsc.Instance->Initialize(entity);
            nsc.Instance->SetEnabled(false); // Start disabled
            std::cout << "Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
        }
    }

    void ScriptSystem::OnEntityRemoved(Entity entity) {
        // should probably call OnScriptComponentDestroyed here if the entity
        // has a script component, to ensure proper cleanup.
        (void)entity;
    }

    void ScriptSystem::Init() {
        // Logic to initialize the system when a scene loads
        scriptingEngine = std::make_unique<Scripting::ScriptingEngine>();
        //Scripting Test
        const std::string dllPath = "GameCode.dll";
        bool loaded = scriptingEngine->LoadGameDLL(dllPath);

        if (!loaded) {
            std::cerr << "TEST FAILED: Could not load " << dllPath << std::endl;
            std::cerr << "Error: " << scriptingEngine->GetLastError() << std::endl;
        }
        else {
            std::cout << "DLL loaded successfully." << std::endl;

            scriptingEngine->PrintSummary();
           
        }
    }

    void ScriptSystem::Exit() {
        // Logic to clean up the system when a scene unloads
		for (Entity entity : m_componentManager->GetEntitiesWithComponent<Component::NativeScript>()) {
			OnScriptComponentDestroyed(entity);
		}

        std::cout << "\n--- SCRIPT SYSTEM TEST COMPLETE ---\n" << std::endl;
        scriptingEngine->Shutdown(); //tmp
    }

    void ScriptSystem::StartScripts()
    {
        std::cout << "ScriptSystem: Entering Play Mode..." << std::endl;
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

            // Only create if the component is valid and not already instantiated
            //if (nsc.CreateScript && !nsc.Instance) {
            //    nsc.Instance = nsc.CreateScript();
            //    nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
            //    nsc.Instance->SetEntity(entity);
            //    nsc.Instance->Initialize(entity);
            //    std::cout << "Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
            //}
            //else if(nsc.Instance){
			if (nsc.Instance)
				nsc.Instance->SetEnabled(true);
            //}
        }
    }

    void ScriptSystem::PauseScripts()
    {
		std::cout << "ScriptSystem: Pausing Play Mode..." << std::endl;
		const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

		for (Entity entity : entities) {
			auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance) {
				nsc.Instance->SetEnabled(false);
				std::cout << "Paused script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
			}
		}
    }

    void ScriptSystem::StopScripts()
    {
        std::cout << "ScriptSystem: Exiting Play Mode..." << std::endl;
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

            if (nsc.Instance) {
                nsc.Instance->OnDestroy();
                if (nsc.DestroyScript) {
                    nsc.DestroyScript(nsc.Instance);
                }
                else {
                    delete nsc.Instance; // Fallback
                }
                // CRITICAL: Reset the instance pointer to null
                nsc.Instance = nullptr;
                std::cout << "Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
            }

			// Recreate the script instance for the next play session
            if (nsc.CreateScript && !nsc.Instance) {
                nsc.Instance = nsc.CreateScript();
                nsc.Instance->LinkToEngine(m_componentManager); // Link to engine systems
                nsc.Instance->SetEntity(entity);
                nsc.Instance->Initialize(entity);
				nsc.Instance->SetEnabled(false); // Start disabled
                std::cout << "Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
            }
        }
    }
    

    void ScriptSystem::Update(double deltaTime) {
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        // First loop: Instantiate and Initialize new scripts
        //for (Entity entity : entities) {
        //    auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

        //    if (nsc.CreateScript && !nsc.Instance) {
        //        // Instantiate the script
        //        nsc.Instance = nsc.CreateScript();

        //        // Give the script a handle to its entity owner
        //        nsc.Instance->SetEntity(entity);

        //        nsc.Instance->LinkToEngine(m_componentManager);

        //        nsc.Instance->Initialize(entity);

        //        std::cout << "Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
        //    }
        //}

        // Second loop: Update all active scripts
        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
            if (nsc.Instance && nsc.Instance->IsEnabled()) {
                nsc.Instance->Update(deltaTime);
            }
        }
    }

    void ScriptSystem::OnScriptComponentDestroyed(Entity entity) {
        auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);
        if (nsc.Instance) {
            nsc.Instance->OnDestroy();
            if (nsc.DestroyScript) {
                nsc.DestroyScript(nsc.Instance);
            }
            else {
                delete nsc.Instance; // Fallback
            }
            nsc.Instance = nullptr;
            std::cout << "Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
        }
    }
}
