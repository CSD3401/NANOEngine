#pragma once

#include "ECS/Core/System.hpp"
#include "ECS/Core/ComponentManager.hpp"

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS::Systems {
	class DecalProjectorSystem final : public System {
	public:
		explicit DecalProjectorSystem(ComponentManager* cm, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

	private:
		ComponentManager* m_componentManager;
		Core::LUIDRegistry* m_luidRegistry;
	};
}