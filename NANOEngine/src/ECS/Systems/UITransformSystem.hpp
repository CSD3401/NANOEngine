#ifndef UI_TRANSFORM_SYSTEM_HPP
#define UI_TRANSFORM_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include <unordered_map>
#include <vector>

namespace NE::ECS::Systems {

    class UITransformSystem : public System 
    {
    public:
        UITransformSystem(ComponentManager* cm);

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void Init() override;
        void Update(double dt) override;
        void Exit() override;

    private:
        ComponentManager* m_cm;
    };

} // namespace NE::ECS::Systems

#endif // UI_TRANSFORM_SYSTEM_HPP
