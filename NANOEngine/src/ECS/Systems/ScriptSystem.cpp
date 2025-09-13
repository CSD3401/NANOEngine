#include "ScriptSystem.hpp" 
#include <iostream>
#include "../Components/NativeScript.hpp"

namespace NE::ECS::Systems {

    ScriptSystem::ScriptSystem(ComponentManager* cm)
        : m_componentManager(cm) {
        // Constructor body can be empty if you just need to initialize members
    }

    // === ADD THESE MISSING LIFECYCLE FUNCTIONS ===
    // Even if they are empty, they need to be defined
    void ScriptSystem::OnEntityAdded(Entity entity) {
        // Logic for when an entity relevant to the script system is added

        entity;
    }

    void ScriptSystem::OnEntityRemoved(Entity entity) {
        // You should probably call OnScriptComponentDestroyed here if the entity
        // has a script component, to ensure proper cleanup.
        entity;
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

        std::cout << "\n--- SCRIPT SYSTEM TEST COMPLETE ---\n" << std::endl;
        scriptingEngine->Shutdown(); //tmp
    }

    void ScriptSystem::Update(double deltaTime) {
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        // First loop: Instantiate and Initialize new scripts
        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

            if (nsc.CreateScript && !nsc.Instance) {
                // Instantiate the script
                nsc.Instance = nsc.CreateScript();

                // Give the script a handle to its entity owner
                nsc.Instance->SetEntity(entity);

                nsc.Instance->Initialize(entity);

                std::cout << "Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
            }
        }

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
