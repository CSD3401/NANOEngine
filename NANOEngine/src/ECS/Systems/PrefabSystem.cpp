#include "pch.h"
#include "PrefabSystem.hpp"

#include "../Core/ComponentManager.hpp"
#include "../Components/PrefabInstance.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::ECS::Systems {

	PrefabSystem::PrefabSystem(ComponentManager* cm) 
		: m_componentManager(cm) { }

	void PrefabSystem::OnEntityAdded(Entity entity) {
		// Push prefabInstance into prefabmanager 
		Prefab::PrefabManager::Instantiate(
			m_componentManager->GetComponent<Component::PrefabInstance>(entity).prefabUUID,
			entity);

		SPD_DEBUG("PrefabSystem: OnEntityAdded called for entity: " << entity);	
	}

	void PrefabSystem::OnEntityRemoved(Entity entity) {
		Prefab::PrefabManager::DestroyInstance(
			m_componentManager->GetComponent<Component::PrefabInstance>(entity).prefabUUID,
			entity);
	}

	void PrefabSystem::Init() {
	}

	void PrefabSystem::Update(double /*deltaTime*/) {
	}

	void PrefabSystem::Exit() {
	}
}
