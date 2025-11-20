#pragma once

// Force recompile timestamp: 2025-01-09

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
//#include "../src/Scripting/ScriptingEngine.hpp"
#include "FileWatch.hpp"
#include <queue>
#include <mutex>
#include <atomic>

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

		void InitializeExistingScripts();

		void StartScripts(); //Play
		void PauseScripts(); //Pause
		void StopScripts(); //Stop


	private:
		ComponentManager* m_componentManager;
	};

}