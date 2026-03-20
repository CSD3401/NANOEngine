#ifndef UI_EVENT_SYSTEM_HPP
#define UI_EVENT_SYSTEM_HPP

#include "ECS/Core/System.hpp"
#include "ECS/Components/UICanvas.hpp"
#include "ECS/Components/UIRectTransform.hpp"
#include "ECS/Components/UIButton.hpp"
#include "ECS/Components/UISlider.hpp"
#include "ECS/Components/UIToggle.hpp"
#include "ECS/Components/UIImage.hpp"
#include "ECS/Components/UIScrollRect.hpp"
#include "ECS/Components/UIInputField.hpp"
#include "ECS/Components/UIDropdown.hpp"
#include "UILayoutEngine.hpp"
#include "UILayoutSystem.hpp"
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Math/Mat4.hpp"
#include "Math/Vec2.hpp"
#include <vector>

namespace NE::ECS {
    class ComponentManager;
    class EntityManager;
}

namespace NE::ECS::Systems {

    class UIEventSystem final : public System {
    public:
        explicit UIEventSystem(ComponentManager* cm, EntityManager* em);

        void SetLayoutEngine(UILayoutEngine* engine) { m_layoutEngine = engine; }
        void SetLayoutSystem(UILayoutSystem* system) { m_layoutSystem = system; }

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void OnEntityActive(Entity entity) override;
        void OnEntityInactive(Entity entity) override;
        Entity GetHoveredEntity() const { return m_hoveredEntity; }
        Entity GetPressedEntity() const { return m_pressedEntity; }
        Entity GetFocusedEntity() const { return m_focusedEntity; }

        // Programmatic focus control
        void SetFocusedEntity(Entity e);
        void ClearFocus();

        // Viewport bounds for Editor (transforms mouse from window coords to UI coords)
        static void SetViewportBounds(float offsetX, float offsetY, float width, float height, float uiWidth, float uiHeight);
        static void ClearViewportBounds();

    private:
        bool IsActiveForUI(Entity entity, Entity canvasEntity) const;
        Entity FindOwningCanvas(Entity entity) const;
        ComponentManager* m_cm = nullptr;
        EntityManager* m_em = nullptr;
        UILayoutEngine* m_layoutEngine = nullptr;
        UILayoutSystem* m_layoutSystem = nullptr;

        Entity m_hoveredEntity = NO_ENTITY;
        Entity m_pressedEntity = NO_ENTITY;
        Entity m_focusedEntity = NO_ENTITY;
        Entity m_draggingSlider = NO_ENTITY;

        // Drag event state
        float m_lastDragX = 0.0f;
        float m_lastDragY = 0.0f;
        bool m_isDragging = false;

        struct UIElementInfo {
            Entity entity;
            float worldX;
            float worldY;
            float worldWidth;
            float worldHeight;
            float zOrder;
            Entity canvasEntity;
            bool isWorldSpace = false;
            // Rotation for screen-space hit testing (accumulated rotation in degrees)
            float rotationZ = 0.0f;
            float pivotX = 0.5f;
            float pivotY = 0.5f;
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

        // Focus management
        void HandleFocusChange(Entity clickedEntity, bool mousePressed);
        Entity FindNextFocusable(Entity current, bool reverse = false);

        // Input field handling
        void UpdateInputFields(double deltaTime);
        void ProcessInputFieldKeyboard(Entity entity, Component::UIInputField& field, double deltaTime);
        void ApplyInputFieldColorToImage(Entity entity);
        void SyncInputFieldToText(Entity entity);
        bool IsCharAllowed(char32_t codepoint, const Component::UIInputField& field);
        void DeleteSelection(Component::UIInputField& field);
        void InsertText(Component::UIInputField& field, const std::string& text);

        // Drag events
        void UpdateDragEvents(float mouseX, float mouseY, bool mouseDown, bool mousePressed, bool mouseReleased);

        // Dropdown handling
        Entity m_expandedDropdown = NO_ENTITY;
        void UpdateDropdowns(float mouseX, float mouseY, bool mousePressed, bool mouseReleased);
        void ExpandDropdown(Entity dropdownEntity);
        void CollapseDropdown();
        void SyncDropdownOptionsToPanel(Entity dropdownEntity);
        void SyncDropdownCaptionText(Entity dropdownEntity);
        void ApplyDropdownColorToImage(Entity dropdownEntity);
        bool IsEntityInDropdownPanel(Entity entity, Entity dropdownEntity);
        int GetOptionIndexFromEntity(Entity clickedEntity, Entity dropdownEntity);
    };

} // namespace NE::ECS::Systems

#endif // UI_EVENT_SYSTEM_HPP
