#pragma once
#include "../Core/System.hpp"

namespace NE::ECS {
    class ComponentManager;
    class EntityManager;
}

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE::ECS::Systems {

    class AnimatorSystem final : public System {
    public:
        explicit AnimatorSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr);

        void OnEntityAdded(Entity) override;
        void OnEntityRemoved(Entity) override;

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;

    private:
        ComponentManager* m_componentManager;
        EntityManager* m_entityManager;
        Core::LUIDRegistry* m_luidRegistry;
    };

} // namespace NE::ECS::Systems
