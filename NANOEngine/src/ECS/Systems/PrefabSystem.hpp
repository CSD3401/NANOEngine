#pragma once

#include "../Core/System.hpp"

namespace NE::ECS {
	class ComponentManager;
}

namespace NE::ECS::Systems {

	class PrefabSystem final : public System {
	public:
		explicit PrefabSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;
		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

	private:
		ComponentManager* m_componentManager;
	};

}