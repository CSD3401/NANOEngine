#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"

namespace NE::ECS::Systems {

    class UIRenderSystem final : public System {
    public:
        explicit UIRenderSystem(ComponentManager* cm);

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void Init() override;
        void Update(double deltaTime) override; // submits UI commands to GraphicsManager
        void Exit() override;

    private:
        ComponentManager* m_cm = nullptr;
    };

} // namespace NE::ECS::Systems
#endif // END UI_RENDER_SYSTEM_HPP
