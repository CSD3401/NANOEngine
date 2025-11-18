#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"

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

        struct WorldTransform {
            float x, y, width, height;
        };

        // calculates final screen positions for UI elements
        WorldTransform CalculateWorldTransform(Entity entity, const NE::ECS::Component::UICanvas& canvas);

        // handles the different scaling mode
        float CalculateScaleFactor(const NE::ECS::Component::UICanvas& canvas);

        // Renders all chlildren of a canvas
        void RenderCanvasChildren(Entity canvasEntity, const NE::ECS::Component::UICanvas& canvas);
    };

} // namespace NE::ECS::Systems
#endif // END UI_RENDER_SYSTEM_HPP
