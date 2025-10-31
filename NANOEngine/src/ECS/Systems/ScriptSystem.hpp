#pragma once

// Force recompile timestamp: 2025-01-09

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../src/Scripting/ScriptingEngine.hpp"

// Forward declaration to avoid circular dependency
namespace NE::ECS::Component {
    struct NativeScript;
}

namespace NE::ECS::Systems {

	class ScriptSystem final : public System {
	public:
		explicit ScriptSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

		void StartScripts(); //Play
		void PauseScripts(); //Pause
		void StopScripts(); //Stop

		void OnScriptComponentDestroyed(Entity entity);

        /**
         * Saves all exposed field values from a script instance to SerializedFields.
         * Called when saving scenes or when stopping play mode.
         */
        void SaveSerializedFields(NE::ECS::Component::NativeScript& nsc);

        /**
         * Restores field values from SerializedFields to a script instance.
         * Called when loading scenes or creating script instances.
         */
        void RestoreSerializedFields(NE::ECS::Component::NativeScript& nsc);

		Scripting::ScriptingEngine* GetScriptingEngine() const { return scriptingEngine.get(); }

	private:
		ComponentManager* m_componentManager;
		std::unique_ptr<Scripting::ScriptingEngine> scriptingEngine; // Pointer to the scripting engine
	};

}