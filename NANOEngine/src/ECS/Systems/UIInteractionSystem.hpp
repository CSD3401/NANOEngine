#ifndef UI_INTERACTION_SYSTEM_HPP
#define UI_INTERACTION_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIButton.hpp"
#include "UITransformSystem.hpp"
#include "../../Input/InputManager.hpp"
#include <unordered_map>

namespace NE::ECS::Systems {

    class UIInteractionSystem : public System {
    public:
        //=================================================================
        // Lifecycle
        //=================================================================

        explicit UIInteractionSystem(ComponentManager* cm, UITransformSystem* transformSystem = nullptr);

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void SetTransformSystem(UITransformSystem* transformSystem);
        static void SetViewportBounds(float x, float y, float width, float height);
        static void SetCameraMatrices(const NE::Math::Mat4& view, const NE::Math::Mat4& projection);

    private:
        ComponentManager* m_cm = nullptr;
        UITransformSystem* m_transformSystem = nullptr;

        //=================================================================
        // Interaction Detection
        //=================================================================

        bool IsPointInRect(
            double mouseX, 
            double mouseY,
            const UITransformSystem::WorldTransform& worldTransform,
            const Component::UIRectTransform& rect,
            float rotationDegrees
        );

        Entity FindCanvasForEntity(Entity entity);

        void UpdateButtonState(
            Entity entity,
            bool isHovering,
            bool isPressed
        );
        
        void ProcessButtonClicks();
        void ProcessScreenSpaceButtons(Entity canvasEntity);
        void ProcessWorldSpaceButtons(Entity canvasEntity);
        
        struct Ray {
            NE::Math::Vec3 origin;
            NE::Math::Vec3 direction;
        };
        
        Ray ScreenToRay(double mouseX, double mouseY, 
                       const NE::Math::Mat4& viewMatrix, 
                       const NE::Math::Mat4& projMatrix,
                       float viewportX, float viewportY, 
                       float viewportWidth, float viewportHeight);
        
        bool RayIntersectsUIElement(
            const Ray& ray,
            const UITransformSystem::WorldTransform& worldTransform,
            const Component::UIRectTransform& rect,
            const UITransformSystem::AccumulatedTransform& accumulated,
            NE::Math::Vec3& outIntersectionPoint
        );

        std::unordered_map<Entity, bool> m_wasPressedLastFrame;
        std::unordered_map<Entity, bool> m_wasPressedOnButton;
        
        static float s_viewportX;
        static float s_viewportY;
        static float s_viewportWidth;
        static float s_viewportHeight;
        static bool s_viewportSet;
        
        static NE::Math::Mat4 s_cameraView;
        static NE::Math::Mat4 s_cameraProjection;
        static bool s_cameraMatricesSet;
    };

} // namespace NE::ECS::Systems

#endif // UI_INTERACTION_SYSTEM_HPP
