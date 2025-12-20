#include "EntityManager.hpp"
#include <cassert>

namespace NE::ECS {

	EntityManager::EntityManager() {
		m_availableEntities.reserve(MAX_ENTITIES);
		for (int i = static_cast<int>(MAX_ENTITIES) - 1; i >= 0; --i) {
			m_availableEntities.push_back(static_cast<Entity>(i));
		}
		m_usedEntities.reserve(MAX_ENTITIES);
		m_layer.resize(MAX_ENTITIES, Core::LayerID{ 0 });
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
		auto it = std::find_if(m_usedEntities.begin(), m_usedEntities.end(),
			[id = entity](const Entity& entt) {
				return entt == id;
			});

		if (it != m_usedEntities.end()) {
			m_usedEntities.erase(it);
		}

		m_signatures[entity].reset();
		m_layer[entity] = Core::LayerID{ 0 };
		m_availableEntities.push_back(entity);
	}

	Signature EntityManager::GetSignature(Entity entity) {
		return m_signatures[entity];
	}

	void EntityManager::SetSignature(Entity entity, Signature sig) {
		m_signatures[entity] = sig;
	}

	Core::LayerID EntityManager::GetLayer(Entity entity) const noexcept {
		return m_layer[entity];
	}

	void EntityManager::SetLayer(Entity entity, Core::LayerID layer) noexcept {
		m_layer[entity] = layer;
	}

	Core::LayerMask EntityManager::GetLayerBit(Entity entity) const noexcept {
		return Core::LayerMask{ 1 } << static_cast<uint8_t>(m_layer[entity]);
	}

}