#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../src/Math/Mat4.hpp"

namespace NE::ECS::Systems {

    class UIRenderSystem final : public System {
    public:

        struct WorldTransform {
            float x, y, z;
            float width, height;
        };

        explicit UIRenderSystem(ComponentManager* cm);

        void Init() override;
        void Update(double deltaTime) override; // submits UI commands to GraphicsManager
        void Exit() override;

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

    private:
        ComponentManager* m_cm = nullptr;

        // calculate world transforms based on render mode
        WorldTransform CalculateWorldTransform(
            Entity entity,
            const NE::ECS::Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix = nullptr,
            const Math::Mat4* projMatrix = nullptr
        );

        // handles the different scaling mode
        float CalculateScaleFactor(const NE::ECS::Component::UICanvas& canvas);

        // renders all chlildren of a canvas
        void RenderCanvasChildren(
            Entity canvasEntity, 
            const NE::ECS::Component::UICanvas& canvas,
            const Math::Mat4* viewMatrix = nullptr,
            const Math::Mat4* projMatrix = nullptr
        );

        // helper:: get camera matrices
        bool GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj);
    };

} // namespace NE::ECS::Systems
#endif // END UI_RENDER_SYSTEM_HPP
