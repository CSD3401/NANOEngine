#pragma once

#include "../Core/System.hpp"
#include <vector>
#include <unordered_map>
#include "../Core/ComponentManager.hpp"

namespace NE::Math {
	struct Mat4;
}

namespace NE::ECS::Systems {
	class TransformSystem final : public System {
	public:
		explicit TransformSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;


		void SetParent(Entity child, Entity newParent, bool keepWorld = true);
	private:
		void MarkDirtyRecursive(Entity e);
		void BuildLocalMatrices();
		void UpdateWorldRecursive(Entity e, const Math::Mat4& parentWorld);

		void ResolvePendingParentsForAll(bool keepWorldForNewParents);

		struct PendingParent {
			Entity      child;
			uint64_t    parentLuid;
		};

		std::unordered_map<uint64_t, Entity> m_luidToEntity;
		std::vector<PendingParent>  m_pendingParents;

		ComponentManager* m_componentManager;
	};
}


