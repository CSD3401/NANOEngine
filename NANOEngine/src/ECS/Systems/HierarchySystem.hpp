#pragma once

#include "../Core/System.hpp"
#include "ECS/Core/ComponentManager.hpp"

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS::Systems {
	class HierarchySystem final : public System {
	public:
		explicit HierarchySystem(ComponentManager* cm, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity e) override;
		void OnEntityRemoved(Entity e) override;

		void OnEntityActive(Entity entity) override;
		void OnEntityInactive(Entity entity) override;
		void Init() override;
		void Update(double) override;
		void Exit() override;

		void SetParent(Entity child, Entity newParent, bool keepWorld = true);
		void SetParent(Entity child,
			Entity newParent,
			int insertIndex,
			bool keepWorld = true);
		void SetActive(Entity root, bool isActive);

	private:
		void ResolvePendingParentsForAll(bool keepWorldForNewParents);

		struct PendingParent {
			Entity   child;
			uint64_t parentLuid;
		};

		std::unordered_map<uint64_t, Entity> m_luidToEntity;
		std::vector<PendingParent>  m_pendingParents;
		ComponentManager* m_componentManager;
		Core::LUIDRegistry* m_luidRegistry;
	};
}