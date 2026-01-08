#ifndef RECT_TRANSFORM_SYSTEM_HPP
#define RECT_TRANSFORM_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE::ECS::Systems {

    class RectTransformSystem final : public System {
    public:
        explicit RectTransformSystem(ComponentManager* cm, Core::LUIDRegistry* lr);

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void Init() override;
        void Update(double dt) override;
        void Exit() override;

    private:
        ComponentManager* m_componentManager;
        Core::LUIDRegistry* m_luidRegistry;
    };

}

#endif