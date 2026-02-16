#ifndef UI_EVENT_SYSTEM_HPP
#define UI_EVENT_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIButton.hpp"
#include "../Components/UISlider.hpp"
#include "../Components/UIToggle.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIRectMask2D.hpp"
#include "../Components/UIScrollRect.hpp"
#include "UILayoutEngine.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec2.hpp"
#include <vector>

namespace NE::ECS::Systems {

    class UIEventSystem final : public System {
    public:
        explicit UIEventSystem(ComponentManager* cm);

        void SetLayoutEngine(UILayoutEngine* engine) { m_layoutEngine = engine; }

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        Entity GetHoveredEntity() const { return m_hoveredEntity; }
        Entity GetPressedEntity() const { return m_pressedEntity; }

        // Viewport bounds for Editor (transforms mouse from window coords to UI coords)
        static void SetViewportBounds(float offsetX, float offsetY, float width, float height, float uiWidth, float uiHeight);
        static void ClearViewportBounds();

    private:
        bool IsActiveForUI(Entity entity, Entity canvasEntity) const;
        Entity FindOwningCanvas(Entity entity) const;
        ComponentManager* m_cm = nullptr;
        UILayoutEngine* m_layoutEngine = nullptr;

        Entity m_hoveredEntity = NO_ENTITY;
        Entity m_pressedEntity = NO_ENTITY;
        Entity m_draggingSlider = NO_ENTITY;

        struct UIElementInfo {
            Entity entity;
            float worldX;
            float worldY;
            float worldWidth;
            float worldHeight;
            float zOrder;
            Entity canvasEntity;
            bool isWorldSpace = false;
            // World-space specific data
            Math::Vec3 worldPosition;
            Math::Vec3 worldNormal;
            float worldSpaceWidth = 0.0f;
            float worldSpaceHeight = 0.0f;
            Math::Mat4 modelMatrix;
        };

        std::vector<UIElementInfo> CollectInteractableElements();
        std::vector<UIElementInfo> CollectWorldSpaceElements();

        bool PointInRect(float px, float py, const UIElementInfo& element);

        // World-space ray casting
        bool GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj);
        Math::Vec3 ScreenToWorldRay(float screenX, float screenY, const Math::Mat4& invViewProj);
        bool RayPlaneIntersect(
            const Math::Vec3& rayOrigin,
            const Math::Vec3& rayDir,
            const Math::Vec3& planePoint,
            const Math::Vec3& planeNormal,
            float& outT,
            Math::Vec3& outHitPoint
        );
        bool IsPointInWorldSpaceElement(
            const Math::Vec3& hitPoint,
            const UIElementInfo& element
        );
        Entity FindWorldSpaceHit(float mouseX, float mouseY);

        void UpdateButtonStates();
        void UpdateSliderStates(float mouseX, float mouseY, bool mouseDown, bool mousePressed, bool mouseReleased);
        void UpdateToggleStates();
        void UpdateCheckmarkVisibility(Entity toggleEntity);
        void UpdateScrollRects(float mouseX, float mouseY, bool mouseDown, bool mousePressed, bool mouseReleased, double deltaTime);

        void ApplyButtonColorToImage(Entity buttonEntity);

        // Mask-aware hit testing (RectMask2D)
        bool IsPointInMaskBounds(Entity entity, float px, float py);

        Entity m_draggingScrollRect = NO_ENTITY;

        float CalculateScaleFactor(const Component::UICanvas& canvas);

        void CalculateWorldRect(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            float& outX, float& outY,
            float& outWidth, float& outHeight
        );

        Math::Mat4 BuildWorldSpaceModelMatrix(
            Entity entity,
            Entity canvasEntity,
            const Component::UIRectTransform& rect
        );

        // Viewport transform (for Editor viewports)
        static bool s_useViewportTransform;
        static float s_viewportOffsetX;
        static float s_viewportOffsetY;
        static float s_viewportWidth;
        static float s_viewportHeight;
        static float s_uiWidth;
        static float s_uiHeight;

        void TransformMouseToUICoords(double& mouseX, double& mouseY);
    };

} // namespace NE::ECS::Systems

#endif // UI_EVENT_SYSTEM_HPP
