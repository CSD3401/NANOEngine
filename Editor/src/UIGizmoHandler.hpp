#pragma once

#include <imgui/imgui.h>
#include <imgui/widgets/imguizmo/ImGuizmo.h>
#include <ECS/Components/UIRectTransform.hpp>
#include "Command/EditorSetTransformCommand.hpp"
#include "Math/Mat4.hpp"
#include <cstdint>

namespace Editor {
    /**
     * @brief Handles UI gizmo interactions for both 2D (screen space) and 3D (world space) UI elements
     */
    class UIGizmoHandler {
    public:
        // === State Queries ===
        static bool IsGizmoActive() { return s_gizmoActive; }
        static void SetOperation(ImGuizmo::OPERATION op) { s_currentOperation = op; }
        static ImGuizmo::OPERATION GetOperation() { return s_currentOperation; }

        // === 3D Gizmo (World Space UI) ===
        static void Begin3DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize);
        static void Update3DGizmo(uint32_t uiEntityId,
            const NE::Math::Mat4& view,
            const NE::Math::Mat4& proj,
            ImVec2 panelPos,
            ImVec2 panelSize);
        static void End3DGizmo(uint32_t uiEntityId);

        // === 2D Gizmo (Screen Space UI) ===
        static void Begin2DGizmo(uint32_t uiEntityId);
        static void Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
            float fbWidth, float fbHeight);
        static void End2DGizmo(uint32_t uiEntityId);

        static void UpdateCommandAfter(const NE::ECS::Component::UIRectTransform& rect);

        // World-space helper (TRS only, no width/height/pivot)
        static NE::Math::Mat4 BuildUIWorldTRS(uint32_t entityId);

    private:
        // === Shared State ===
        static bool s_gizmoActive;
        static ImGuizmo::OPERATION s_currentOperation;
        static uint32_t s_gizmoEntityId;
        static int s_gizmoType; // 0 = none, 1 = 2D, 2 = 3D

        // === 2D Gizmo State - Position/Resize ===
        static bool s_isDraggingUI;
        static int s_draggingCorner;
        static int s_draggingEdge;
        static ImVec2 s_dragStart;
        static NE::ECS::Component::UIRectTransform s_originalTransform;
        static ImVec2 s_originalWorldPos;

        // === 2D Gizmo State - Rotation ===
        static bool s_isDraggingRotation;
        static float s_rotationStartAngle;
        static float s_originalRotation;
        static ImVec2 s_rotationCenter;

        // === Undo/Redo Command State ===
        static std::unique_ptr<SetUIRectTransformCommand> s_uiGizmoCmd;
        static uint8_t s_uiGizmoMask;

        // === Helper Functions ===
        static float GetAngleFromCenter(ImVec2 center, ImVec2 point);
        static void CommitCommand();
    };

} // namespace Editor
