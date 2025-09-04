#include "ScriptSystem.hpp" 
#include <iostream>
#include "../Components/NativeScript.hpp"

namespace NE::ECS::Systems {
  
        void ScriptSystem::Update(double deltaTime) {
            

            const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScriptComponent>();
            for (Entity entity : entities) {

				auto& nsc = m_componentManager->GetComponent<Component::NativeScriptComponent>(entity);
                
                if (nsc.CreateScript && !nsc.Instance) {
                    // Instantiate the script
                    nsc.Instance = nsc.CreateScript();

                    // Give the script a handle to its entity owner
                    nsc.Instance->SetEntity(entity);

                    // Call the creation lifecycle method
                    nsc.Instance->OnCreate();
                    std::cout << "Created script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
                }
            }

            // Update all active scripts
            // Iterate over all entities with an active script instance.
            for (Entity entity : entities) {
                auto& nsc = m_componentManager->GetComponent<Component::NativeScriptComponent>(entity);
                if (nsc.Instance) {
                    nsc.Instance->OnUpdate(deltaTime);
                }
            }
        }

        // You also need to handle script destruction. This is often done by listening
        // for component removal events in your ECS.
        void ScriptSystem::OnScriptComponentDestroyed(Entity entity) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScriptComponent>(entity);
            if (nsc.Instance) {
                nsc.Instance->OnDestroy();
                nsc.DestroyScript(nsc.Instance);
                nsc.Instance = nullptr;
                std::cout << "Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity << std::endl;
            }
        }
}
