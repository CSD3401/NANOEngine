#ifndef NANOENGINE_ECS_ENTITY_MANAGER_HPP
#define NANOENGINE_ECS_ENTITY_MANAGER_HPP

#include <vector>
#include <array>

#include "Entity.hpp"
#include "Component.hpp"
#include "Signature.hpp"
#include "Core/Layers.hpp"

namespace NE::ECS {

	class EntityManager {
	public:
		EntityManager();
		~EntityManager() = default;

		EntityManager(const EntityManager&) = delete;
		EntityManager& operator=(const EntityManager&) = delete;

		Entity CreateEntity();
		Entity CreateEntityWithId(Entity id);
		void DestroyEntity(Entity entity);

		Signature GetSignature(Entity entity);
		void SetSignature(Entity entity, Signature sig);

		void ToggleActive(Entity e, bool active);
		bool GetActive(Entity e);

		std::vector<Entity>& GetUsedEntities() { return m_usedEntities; }

		Core::LayerID GetLayer(Entity entity) const noexcept;
		void SetLayer(Entity entity, Core::LayerID layer) noexcept;
		Core::LayerMask GetLayerBit(Entity entity) const noexcept;
	private:
		std::vector<Core::LayerID> m_layer;
		std::vector<Entity> m_usedEntities{}; // TEMP;
		std::vector<Entity> m_availableEntities{};
		std::array<Signature, MAX_ENTITIES> m_signatures{};

		std::bitset<MAX_ENTITIES> m_active;
	};

}

#endif // !NANOENGINE_ECS_ENTITY_MANAGER_HPP