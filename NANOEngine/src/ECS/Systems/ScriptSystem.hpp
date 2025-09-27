#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../src/Scripting/ScriptingEngine.hpp"


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

		Scripting::ScriptingEngine* GetScriptingEngine() const { return scriptingEngine.get(); }

	private:
		ComponentManager* m_componentManager;
		std::unique_ptr<Scripting::ScriptingEngine> scriptingEngine; // Pointer to the scripting engine
	};

}