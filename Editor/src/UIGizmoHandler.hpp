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

        static void Begin3DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize);
        static void Update3DGizmo(uint32_t uiEntityId);
        static void End3DGizmo(uint32_t uiEntityId);

        static NE::Math::Mat4 BuildUIMatrix(const NE::ECS::Component::UIRectTransform& rect);

        // ============ 2D GIZMO (Screen Space UI) ============

        static void Begin2DGizmo(uint32_t uiEntityId);
        static void Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
            float fbWidth, float fbHeight);
        static void End2DGizmo(uint32_t uiEntityId);

        // ============ COMMON ============

        static bool IsGizmoActive() { return s_gizmoActive; }
        static ImGuizmo::OPERATION GetCurrentOperation() { return s_currentOperation; }
        static void SetOperation(ImGuizmo::OPERATION op) { s_currentOperation = op; }

        // For undo/redo command
        static NE::ECS::Component::UIRectTransform GetOriginalTransform() { return s_originalTransform; }

    private:
        // Global gizmo state
        static bool s_gizmoActive;
        static ImGuizmo::OPERATION s_currentOperation;
        static uint32_t s_gizmoEntityId;
        static int s_gizmoType; // 0 = none, 1 = 2D, 2 = 3D

        // 3D gizmo state
        static NE::Math::Mat4 s_gizmo3DStartMatrix;

        // 2D gizmo state (shared with both)
        static bool s_isDraggingUI;
        static int s_draggingCorner;
        static int s_draggingEdge;
        static ImVec2 s_dragStart;
        static NE::ECS::Component::UIRectTransform s_originalTransform;
        static ImVec2 s_originalWorldPos;
    };

} // namespace Editor

#endif


















