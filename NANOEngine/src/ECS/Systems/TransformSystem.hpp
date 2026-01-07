#pragma once

#include "../Core/System.hpp"
#include <vector>
#include <unordered_map>
#include "../Core/ComponentManager.hpp"

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::Math {
	struct Mat4;
}

namespace NE::ECS::Systems {
	class TransformSystem final : public System {
	public:
		explicit TransformSystem(ComponentManager* cm, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;
	private:
		void BuildLocalMatrices();
		void UpdateWorldRecursive(Entity e,
			const Math::Mat4& parentWorld,
			bool parentWorldDirty);

		ComponentManager* m_componentManager;
		Core::LUIDRegistry* m_luidRegistry;
	};
}


