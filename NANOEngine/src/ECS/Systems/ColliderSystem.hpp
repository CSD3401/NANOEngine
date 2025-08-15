#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"

namespace NE::ECS::Systems {

	class ColliderSystem final : public System {
	public:
		explicit ColliderSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

	private:
		ComponentManager* m_componentManager;
	};

}