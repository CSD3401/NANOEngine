#include "EntityManager.hpp"
#include <cassert>

namespace NE::ECS {

	EntityManager::EntityManager() {
		m_availableEntities.reserve(MAX_ENTITIES);
		//for (Entity i = MAX_ENTITIES - 1; i != static_cast<Entity>(-1); --i) {
		//	m_availableEntities.push_back(i);
		//}
		for (int i = static_cast<int>(MAX_ENTITIES) - 1; i >= 0; --i) {
			m_availableEntities.push_back(static_cast<Entity>(i));
		}
		m_usedEntities.reserve(MAX_ENTITIES);
	}

	Entity EntityManager::CreateEntity() {
		if (m_availableEntities.empty()) {
			assert(false && "No more entities can be created.");
			return NO_ENTITY;
		}

		Entity entity = m_availableEntities.back();
		m_availableEntities.pop_back();
		m_usedEntities.push_back(entity);

		return entity;
	}

	void EntityManager::DestroyEntity(Entity entity) {
		// temp
		auto it = std::find_if(m_usedEntities.begin(), m_usedEntities.end(),
			[id = entity](const Entity& entt) {
				return entt == id;
			});

		if (it != m_usedEntities.end()) {
			m_usedEntities.erase(it);
		}

		m_signatures[entity].reset();
		m_availableEntities.push_back(entity);
	}

	Signature EntityManager::GetSignature(Entity entity) {
		return m_signatures[entity];
	}

	void EntityManager::SetSignature(Entity entity, Signature sig) {
		m_signatures[entity] = sig;
	}

}