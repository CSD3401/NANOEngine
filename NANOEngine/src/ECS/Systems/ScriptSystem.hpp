#pragma once

// Force recompile timestamp: 2025-01-09

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "FileWatch.hpp"
#include <queue>
#include <mutex>
#include <atomic>

// Forward declaration to avoid circular dependency
namespace NE::ECS {
	class EntityManager;
}

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS::Component {
    struct NativeScript;
}

namespace NE::ECS::Systems {

	class ScriptSystem final : public System {
	public:
		explicit ScriptSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void OnEntityActive(Entity entity) override;
		void OnEntityInactive(Entity entity) override;
		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

		void StartScripts(); //Play
		void PauseScripts(); //Pause
		void StopScripts(); //Stop

	private:
		ComponentManager* m_componentManager;
		EntityManager* m_entityManager;
		Core::LUIDRegistry* m_luidRegistry;
	};

}