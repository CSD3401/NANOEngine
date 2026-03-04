#pragma once
#include "../Core/System.hpp"

namespace NE::ECS {
    class ComponentManager;
    class EntityManager;
}

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE::Animation {
    class AnimationClip;
}

namespace NE::ECS::Systems {

    class AnimatorSystem final : public System {
    public:
        explicit AnimatorSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr);

        void OnEntityAdded(Entity entity) override;
        void OnEntityRemoved(Entity entity) override;
        void OnEntityActive(Entity entity) override;
        void OnEntityInactive(Entity entity) override;
        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;

        void ApplyClipAtTime(Entity e, const Animation::AnimationClip& clip, float t);

    private:
        ComponentManager* m_componentManager;
        EntityManager* m_entityManager;
        Core::LUIDRegistry* m_luidRegistry;
    };

} // namespace NE::ECS::Systems
