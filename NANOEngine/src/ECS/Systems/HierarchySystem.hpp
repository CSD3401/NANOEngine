#pragma once

#include "../Core/System.hpp"
#include "ECS/Core/ComponentManager.hpp"

namespace NE::ECS::Systems {
	class HierarchySystem final : public System {
	public:
		explicit HierarchySystem(ComponentManager* cm);

		void OnEntityAdded(Entity e) override;
		void OnEntityRemoved(Entity e) override;

		void Init() override;
		void Update(double) override {}   // often empty
		void Exit() override {}

		void SetParent(Entity child, Entity newParent, bool keepWorld = true);

	private:
		void ResolvePendingParentsForAll(bool keepWorldForNewParents);

		struct PendingParent {
			Entity   child;
			uint64_t parentLuid;
		};

		std::unordered_map<uint64_t, Entity> m_luidToEntity;
		std::vector<PendingParent>  m_pendingParents;
		ComponentManager* m_componentManager;
	};
}