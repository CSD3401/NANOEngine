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

        // Set the transform system (called after both systems are registered)
        void SetTransformSystem(UITransformSystem* transformSystem);
        
        // Set viewport bounds for screen space overlay UI (called by editor)
        // This accounts for the panel/viewport where the UI is rendered
        static void SetViewportBounds(float x, float y, float width, float height);
        
        // Set camera matrices for world space UI interaction (called by editor or scene)
        static void SetCameraMatrices(const NE::Math::Mat4& view, const NE::Math::Mat4& projection);

    private:
        ComponentManager* m_cm = nullptr;
        UITransformSystem* m_transformSystem = nullptr;

        //=================================================================
        // Interaction Detection
        //=================================================================

        // Screen Space UI: Point-in-rectangle detection
        bool IsPointInRect(
            double mouseX, 
            double mouseY,
            const UITransformSystem::WorldTransform& worldTransform,
            const Component::UIRectTransform& rect,
            float rotationDegrees
        );

        // Find the canvas that owns this UI element
        Entity FindCanvasForEntity(Entity entity);

        // Update button state based on interaction
        void UpdateButtonState(
            Entity entity,
            bool isHovering,
            bool isPressed
        );
        
        // Detect and fire onClick events for buttons
        void ProcessButtonClicks();

        // Process all buttons in a canvas (screen space overlay)
        void ProcessScreenSpaceButtons(Entity canvasEntity);

        // Process all buttons in a canvas (world space)
        void ProcessWorldSpaceButtons(Entity canvasEntity);
        
        // World Space UI: Raycast-based detection
        struct Ray {
            NE::Math::Vec3 origin;
            NE::Math::Vec3 direction;
        };
        
        // Convert screen coordinates to a world space ray
        Ray ScreenToRay(double mouseX, double mouseY, 
                       const NE::Math::Mat4& viewMatrix, 
                       const NE::Math::Mat4& projMatrix,
                       float viewportX, float viewportY, 
                       float viewportWidth, float viewportHeight);
        
        // Check if a ray intersects with a UI element plane
        bool RayIntersectsUIElement(
            const Ray& ray,
            const UITransformSystem::WorldTransform& worldTransform,
            const Component::UIRectTransform& rect,
            NE::Math::Vec3& outIntersectionPoint
        );

        // Track which button was pressed last frame (for click detection)
        std::unordered_map<Entity, bool> m_wasPressedLastFrame;
        
        // Track which button was being pressed when mouse was down (for click detection)
        std::unordered_map<Entity, bool> m_wasPressedOnButton;
        
        // Viewport bounds for screen space overlay UI (set by editor)
        static float s_viewportX;
        static float s_viewportY;
        static float s_viewportWidth;
        static float s_viewportHeight;
        static bool s_viewportSet;
        
        // Camera matrices for world space UI interaction (set by editor or scene)
        static NE::Math::Mat4 s_cameraView;
        static NE::Math::Mat4 s_cameraProjection;
        static bool s_cameraMatricesSet;
    };

} // namespace NE::ECS::Systems

#endif // UI_INTERACTION_SYSTEM_HPP
