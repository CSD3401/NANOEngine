#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"

namespace NE::ECS::Systems {
	class TransformSystem final : public System {
	public:
		explicit TransformSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;
	private:
		ComponentManager* m_componentManager;
	};
}


