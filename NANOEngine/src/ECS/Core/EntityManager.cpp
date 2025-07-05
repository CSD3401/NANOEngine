#include "EntityManager.hpp"
#include "../../Core/Logger.hpp"
#include <cassert>

namespace NANOEngine::ECS {

	EntityManager::EntityManager()
	{
		m_availableEntities.reserve(MAX_ENTITIES);
		//for (Entity i = MAX_ENTITIES - 1; i != static_cast<Entity>(-1); --i) {
		//	m_availableEntities.push_back(i);
		//}
		for (int i = static_cast<int>(MAX_ENTITIES) - 1; i >= 0; --i) {
			m_availableEntities.push_back(static_cast<Entity>(i));
		}
	}

	Entity EntityManager::CreateEntity()
	{
		if (m_availableEntities.empty()) {
			assert(false && "No more entities can be created.");
			return NO_ENTITY;
		}

		Entity entity = m_availableEntities.back();
		m_availableEntities.pop_back();

		LOG_INFO("Entity Created: " << entity);

		return entity;
	}

	void EntityManager::DestroyEntity(Entity entity)
	{
		m_signatures[entity].reset();
		m_availableEntities.push_back(entity);
	}

	Signature EntityManager::GetSignature(Entity entity)
	{
		return m_signatures[entity];
	}

	void EntityManager::SetSignature(Entity entity, Signature sig)
	{
		m_signatures[entity] = sig;
	}

}