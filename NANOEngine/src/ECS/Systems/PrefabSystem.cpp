#include "PrefabSystem.hpp"

#include "../Core/ComponentManager.hpp"

namespace NE::ECS::Systems {

	PrefabSystem::PrefabSystem(ComponentManager* cm) 
		: m_componentManager(cm) { }

	void PrefabSystem::OnEntityAdded(Entity entity) {
		// Push prefabInstance into prefabmanager 
	}

	void PrefabSystem::OnEntityRemoved(Entity entity) {
	}

	void PrefabSystem::Init() {
	}

	void PrefabSystem::Update(double deltaTime) {
	}

	void PrefabSystem::Exit() {
	}
}
