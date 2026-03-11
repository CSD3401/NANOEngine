#pragma once

#include "ECS/Core/System.hpp"
#include "Graphics/Core/Frustum.hpp"
#include "Graphics/Core/EditorCamera.hpp"

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS {
	class ComponentManager;
	class EntityManager;
}

namespace NE::ECS::Systems {

    class RenderSystem final : public System {
    public:
		explicit RenderSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void OnEntityActive(Entity entity) override;
		void OnEntityInactive(Entity entity) override;
		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

    private:
        ComponentManager* m_componentManager;
		EntityManager* m_entityManager;
		Core::LUIDRegistry* m_luidRegistry;
    };


}


