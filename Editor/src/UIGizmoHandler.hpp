#ifndef UI_GIZMO_HANDLER_HPP
#define UI_GIZMO_HANDLER_HPP

#include <imgui/imgui.h>
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include "ECS/Components/UIRectTransform.hpp"
#include "Math/Mat4.hpp"
#include "Math/Vec3.hpp"
#include <memory>

namespace Editor {

    class UIGizmoHandler {
    public:
        // ============ 3D GIZMO (World Space UI) ============

        // Initialize 3D gizmo for world space UI
        static void Begin3DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize);

        // Update 3D gizmo during manipulation
        static void Update3DGizmo(uint32_t uiEntityId);

        // End 3D gizmo interaction
        static void End3DGizmo(uint32_t uiEntityId);

        // ============ 2D GIZMO (Screen Space UI) ============

        // Initialize 2D gizmo for screen space UI (overlay/camera)
        static void Begin2DGizmo(uint32_t uiEntityId);

        // Update 2D gizmo during manipulation (handles corner/edge drags)
        static void Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
            float fbWidth, float fbHeight);

        // End 2D gizmo interaction
        static void End2DGizmo(uint32_t uiEntityId);

        // ============ COMMON ============

        // Check if any gizmo is active
        static bool IsGizmoActive() { return s_gizmoActive; }

        // Get current operation
        static ImGuizmo::OPERATION GetCurrentOperation() { return s_currentOperation; }

        // Set operation (TRANSLATE, ROTATE, SCALE)
        static void SetOperation(ImGuizmo::OPERATION op) { s_currentOperation = op; }

    private:
        // Global gizmo state
        static bool s_gizmoActive;
        static ImGuizmo::OPERATION s_currentOperation;
        static uint32_t s_gizmoEntityId;
        static int s_gizmoType; // 0 = none, 1 = 2D, 2 = 3D

        // 3D gizmo state
        static NE::Math::Mat4 s_gizmo3DStartMatrix;

        // 2D gizmo state
        static bool s_isDraggingUI;
        static int s_draggingCorner;
        static int s_draggingEdge;
        static ImVec2 s_dragStart;
        static NE::ECS::Component::UIRectTransform s_originalTransform;
        static ImVec2 s_originalWorldPos;
    };

} // namespace Editor

#endif
