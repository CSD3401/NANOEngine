#pragma once

#include "../Core/System.hpp"

namespace NE::ECS {
	class ComponentManager;
	class EntityManager;
}

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS::Systems {

	class CharacterControllerSystem final : public System {
	public:
		explicit CharacterControllerSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void OnEntityActive(Entity entity) override;
		void OnEntityInactive(Entity entity) override;
		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

	private:
		ComponentManager* m_componentManager;
		EntityManager* m_entityManager;
		Core::LUIDRegistry* m_luidRegistry;
	};

}

