#include "ScriptSystem.hpp" 
#include <iostream>
#include "../Components/NativeScript.hpp"
#include "Core/SpdLogger.hpp"

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
      
      // Call Awake() first (even if disabled)
            nsc.Instance->Awake();
    
   // Then Initialize()
 nsc.Instance->Initialize(entity);
            
        // Restore serialized field values if they exist
       RestoreSerializedFields(nsc);

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
        scriptingEngine = std::make_unique<Scripting::ScriptingEngine>();
        //Scripting Test
        const std::string dllPath = "GameCode.dll";
        bool loaded = scriptingEngine->LoadGameDLL(dllPath);

        if (!loaded) {
            SPD_ERROR("Failed to load " << dllPath);
            SPD_ERROR("Error: " << scriptingEngine->GetLastError());
        }
        else {
            SPD_INFO("DLL loaded successfully: " << dllPath);
            scriptingEngine->PrintSummary();
        }
        
        SPD_INFO("ScriptSystem::Init() - Completed");
    }

    void ScriptSystem::Exit() {
        // Logic to clean up the system when a scene unloads
		for (Entity entity : m_componentManager->GetEntitiesWithComponent<Component::NativeScript>()) {
			OnScriptComponentDestroyed(entity);
		}

        SPD_INFO("Script system shutdown complete");
        scriptingEngine->Shutdown(); //tmp
    }

    void ScriptSystem::StartScripts()
    {
        SPD_INFO("ScriptSystem: Entering Play Mode...");
        const auto& entities = m_componentManager->GetEntitiesWithComponent<Component::NativeScript>();

        for (Entity entity : entities) {
            auto& nsc = m_componentManager->GetComponent<Component::NativeScript>(entity);

			if (nsc.Instance)
				nsc.Instance->SetEnabled(true);
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
       SaveSerializedFields(nsc);
  
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
RestoreSerializedFields(nsc);
            
				nsc.Instance->SetEnabled(false); // Start disabled
    SPD_INFO("Initialized script '" << nsc.ScriptName << "' for entity " << (int)entity);
     }
        }
    }
    

    void ScriptSystem::Update(double deltaTime) {
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
            SPD_INFO("Destroyed script '" << nsc.ScriptName << "' for entity " << (int)entity);
        }
    }

    void ScriptSystem::SaveSerializedFields(NE::ECS::Component::NativeScript& nsc) {
        if (!nsc.Instance) return;

        // Clear existing serialized fields
        nsc.SerializedFields.clear();

        // Get all exposed field names from the script
     auto fieldNames = nsc.Instance->GetExposedFieldNames();
        
    // Save each field's current value as a string
   for (const auto& fieldName : fieldNames) {
      std::string value = nsc.Instance->GetFieldValueAsString(fieldName);
  nsc.SerializedFields[fieldName] = value;
        }

     SPD_DEBUG("Saved " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }

    void ScriptSystem::RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc) {
     if (!nsc.Instance || nsc.SerializedFields.empty()) return;

        // Restore each serialized field value
        for (const auto& [fieldName, value] : nsc.SerializedFields) {
       bool success = nsc.Instance->SetFieldValueFromString(fieldName, value);
     if (!success) {
     SPD_WARNING("Failed to restore field '" << fieldName << "' for script '" << nsc.ScriptName << "'");
        }
        }

     SPD_DEBUG("Restored " << nsc.SerializedFields.size() << " fields for script '" << nsc.ScriptName << "'");
    }
}
