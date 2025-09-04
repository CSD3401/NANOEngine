#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"


namespace NE::ECS::Systems {

	class ScriptSystem final : public System {
	public:
		explicit ScriptSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

		void OnScriptComponentDestroyed(Entity entity);

	private:
		ComponentManager* m_componentManager;
	};

}