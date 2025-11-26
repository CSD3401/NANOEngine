#include "UIGizmoHandler.hpp"
#include "imgui/imgui_internal.h"
#include "EditorInterface/ECSExports.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <iostream>

namespace Editor {

    // Global state
    bool UIGizmoHandler::s_gizmoActive = false;
    ImGuizmo::OPERATION UIGizmoHandler::s_currentOperation = ImGuizmo::TRANSLATE;
    uint32_t UIGizmoHandler::s_gizmoEntityId = 0;
    int UIGizmoHandler::s_gizmoType = 0; // 0 = none, 1 = 2D, 2 = 3D

    // 3D gizmo state
    NE::Math::Mat4 UIGizmoHandler::s_gizmo3DStartMatrix;

    // 2D gizmo state
    bool UIGizmoHandler::s_isDraggingUI = false;
    int UIGizmoHandler::s_draggingCorner = -1;
    int UIGizmoHandler::s_draggingEdge = -1;
    ImVec2 UIGizmoHandler::s_dragStart;
    NE::ECS::Component::UIRectTransform UIGizmoHandler::s_originalTransform;
    ImVec2 UIGizmoHandler::s_originalWorldPos;

    // ============ HELPERS ============

    // Helper: Calculate world position by walking up parent hierarchy
    static ImVec2 CalculateUIWorldPosition(uint32_t entity) {
        auto& rect = NE::ECS::Query::GetUIRectTransform(entity);

        float worldX = rect.x;
        float worldY = rect.y;

        // Walk up parent chain
        uint32_t currentParent = rect.parent;
        while (currentParent != std::numeric_limits<uint32_t>::max()) {
            if (!NE::ECS::Query::HasUIRectTransform(currentParent)) {
                break;
            }

            auto& parentRect = NE::ECS::Query::GetUIRectTransform(currentParent);
            worldX += parentRect.x;
            worldY += parentRect.y;

            currentParent = parentRect.parent;
        }

        return ImVec2(worldX, worldY);
    }

    // ============ 3D GIZMO (World Space UI) ============

    void UIGizmoHandler::Begin3DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize) {
        if (s_gizmoActive) return;

        auto& rect = NE::ECS::Query::GetUIRectTransform(uiEntityId);

        // Build initial matrix
        s_gizmo3DStartMatrix = BuildUIMatrix(rect);
        s_gizmoEntityId = uiEntityId;
        s_gizmoActive = true;
        s_gizmoType = 2; // 3D gizmo

        // Store original transform for undo/redo
        s_originalTransform = rect;

        // Setup ImGuizmo
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(panelPos.x, panelPos.y, panelSize.x, panelSize.y);
    }

    void UIGizmoHandler::Update3DGizmo(uint32_t uiEntityId) {
        if (!s_gizmoActive || s_gizmoType != 2 || s_gizmoEntityId != uiEntityId) return;

        auto& rect = NE::ECS::Command::GetUIRectTransform(uiEntityId);
        NE::Math::Mat4 currentMatrix = BuildUIMatrix(rect);

        float matrix[16];
        memcpy(matrix, currentMatrix.Data(), sizeof(float) * 16);

        // This requires you to pass camera from ScenePanel
        // For now using null - you'll need to refactor to pass camera
        bool editedThisFrame = ImGuizmo::Manipulate(
            nullptr,  // view matrix - pass from caller
            nullptr,  // proj matrix - pass from caller
            s_currentOperation,
            ImGuizmo::LOCAL,
            matrix
        );

        if (editedThisFrame && ImGuizmo::IsUsing()) {
            // Convert matrix back to UIRectTransform
            float tr[3], rotDeg[3], sc[3];
            ImGuizmo::DecomposeMatrixToComponents(matrix, tr, rotDeg, sc);

            rect.x = tr[0];
            rect.y = tr[1];
            rect.z = tr[2];

            rect.rotationX = rotDeg[0];
            rect.rotationY = rotDeg[1];
            rect.rotationZ = rotDeg[2];

            rect.scaleX = sc[0];
            rect.scaleY = sc[1];
            rect.scaleZ = sc[2];
        }
    }

    void UIGizmoHandler::End3DGizmo(uint32_t uiEntityId) {
        if (!s_gizmoActive || s_gizmoType != 2 || s_gizmoEntityId != uiEntityId) return;

        s_gizmoActive = false;
        s_gizmoType = 0;
        s_gizmoEntityId = 0;
    }

    NE::Math::Mat4 UIGizmoHandler::BuildUIMatrix(const NE::ECS::Component::UIRectTransform& rect) {
        NE::Math::Mat4 T = NE::Math::Mat4::BuildTranslation(rect.x, rect.y, rect.z);
        NE::Math::Mat4 R = rect.GetRotationMatrix();  // Already handles degree?radian conversion
        NE::Math::Mat4 S = NE::Math::Mat4::BuildScaling(rect.scaleX, rect.scaleY, rect.scaleZ);
        return T * R * S;
    }

    // ============ 2D GIZMO (Screen Space UI) ============

    void UIGizmoHandler::Begin2DGizmo(uint32_t uiEntityId) {
        if (s_gizmoActive) return;

        s_gizmoEntityId = uiEntityId;
        s_gizmoActive = true;
        s_gizmoType = 1; // 2D gizmo

        // Store original transform for undo/redo command
        s_originalTransform = NE::ECS::Query::GetUIRectTransform(uiEntityId);
        s_originalWorldPos = CalculateUIWorldPosition(uiEntityId);

        s_isDraggingUI = false;
        s_draggingCorner = -1;
        s_draggingEdge = -1;
    }

    void UIGizmoHandler::Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
        float fbWidth, float fbHeight) {
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        auto& rectTransform = NE::ECS::Command::GetUIRectTransform(uiEntityId);
        ImVec2 worldPos = CalculateUIWorldPosition(uiEntityId);

        float scaleX = panelSize.x / fbWidth;
        float scaleY = panelSize.y / fbHeight;

        ImVec2 topLeft(
            panelPos.x + worldPos.x * scaleX,
            panelPos.y + worldPos.y * scaleY
        );
        ImVec2 bottomRight(
            panelPos.x + (worldPos.x + rectTransform.width) * scaleX,
            panelPos.y + (worldPos.y + rectTransform.height) * scaleY
        );
        ImVec2 center(
            (topLeft.x + bottomRight.x) * 0.5f,
            (topLeft.y + bottomRight.y) * 0.5f
        );

        ImVec2 corners[4] = {
            topLeft,
            ImVec2(bottomRight.x, topLeft.y),
            bottomRight,
            ImVec2(topLeft.x, bottomRight.y)
        };

        ImVec2 edges[4] = {
            ImVec2(center.x, topLeft.y),
            ImVec2(bottomRight.x, center.y),
            ImVec2(center.x, bottomRight.y),
            ImVec2(topLeft.x, center.y)
        };

        const float handleSize = 8.0f;
        ImVec2 mousePos = ImGui::GetMousePos();

        auto mouseInPanel = [&](ImVec2 p) {
            return p.x >= panelPos.x && p.x <= panelPos.x + panelSize.x &&
                p.y >= panelPos.y && p.y <= panelPos.y + panelSize.y;
            };

        bool mouseInThisPanel = mouseInPanel(mousePos);

        // Hovering detection
        if (!s_isDraggingUI && s_draggingCorner < 0 && s_draggingEdge < 0 && mouseInThisPanel)
        {
            bool hoveringHandle = false;

            // Check corner hover
            for (int i = 0; i < 4; ++i)
            {
                float dx = mousePos.x - corners[i].x;
                float dy = mousePos.y - corners[i].y;
                float dist2 = dx * dx + dy * dy;
                float cornerRadius = handleSize * 0.5f;

                if (dist2 <= cornerRadius * cornerRadius)
                {
                    switch (i) {
                    case 0: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE); break;
                    case 1: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW); break;
                    case 2: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE); break;
                    case 3: ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW); break;
                    }
                    hoveringHandle = true;
                    break;
                }
            }

            // Check edge hover
            if (!hoveringHandle)
            {
                for (int i = 0; i < 4; ++i)
                {
                    float dx = mousePos.x - edges[i].x;
                    float dy = mousePos.y - edges[i].y;
                    float dist2 = dx * dx + dy * dy;
                    float edgeRadius = handleSize * 0.5f;

                    if (dist2 <= edgeRadius * edgeRadius)
                    {
                        switch (i) {
                        case 0: case 2:
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                            break;
                        case 1: case 3:
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            break;
                        }
                        hoveringHandle = true;
                        break;
                    }
                }
            }

            // Check center hover
            if (!hoveringHandle)
            {
                float dx = mousePos.x - center.x;
                float dy = mousePos.y - center.y;
                float dist2 = dx * dx + dy * dy;

                if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                    hoveringHandle = true;
                }
            }
        }

        // Handle clicking
        if (!s_isDraggingUI && s_draggingCorner < 0 && mouseInThisPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            bool handleClicked = false;

            // Check corners
            for (int i = 0; i < 4; ++i)
            {
                float dx = mousePos.x - corners[i].x;
                float dy = mousePos.y - corners[i].y;
                float dist2 = dx * dx + dy * dy;

                if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                {
                    s_draggingCorner = i;
                    s_dragStart = mousePos;
                    s_originalTransform = rectTransform;
                    s_originalWorldPos = worldPos;
                    handleClicked = true;
                    break;
                }
            }

            // Check edges
            if (!handleClicked)
            {
                for (int i = 0; i < 4; ++i)
                {
                    float dx = mousePos.x - edges[i].x;
                    float dy = mousePos.y - edges[i].y;
                    float dist2 = dx * dx + dy * dy;

                    if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                    {
                        s_draggingEdge = i;
                        s_dragStart = mousePos;
                        s_originalTransform = rectTransform;
                        s_originalWorldPos = worldPos;
                        handleClicked = true;
                        break;
                    }
                }
            }

            // Check center
            if (!handleClicked)
            {
                float dx = mousePos.x - center.x;
                float dy = mousePos.y - center.y;
                float dist2 = dx * dx + dy * dy;

                if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                {
                    s_isDraggingUI = true;
                    s_dragStart = mousePos;
                    s_originalTransform = rectTransform;
                    s_originalWorldPos = worldPos;
                }
            }
        }

        // Perform dragging
        if (s_isDraggingUI && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);
            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            float newWorldX = s_originalWorldPos.x + deltaFBX;
            float newWorldY = s_originalWorldPos.y + deltaFBY;

            ImVec2 parentWorldPos(0.0f, 0.0f);
            if (rectTransform.parent != std::numeric_limits<uint32_t>::max() &&
                NE::ECS::Query::HasUIRectTransform(rectTransform.parent)) {
                parentWorldPos = CalculateUIWorldPosition(rectTransform.parent);
            }

            rectTransform.x = newWorldX - parentWorldPos.x;
            rectTransform.y = newWorldY - parentWorldPos.y;
        }

        if (s_isDraggingUI && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingUI = false;
        }

        // Perform corner resize
        if (s_draggingCorner >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);
            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            ImVec2 parentWorldPos(0.0f, 0.0f);
            if (rectTransform.parent != std::numeric_limits<uint32_t>::max() &&
                NE::ECS::Query::HasUIRectTransform(rectTransform.parent)) {
                parentWorldPos = CalculateUIWorldPosition(rectTransform.parent);
            }

            switch (s_draggingCorner) {
            case 0: // Top-left
            {
                float newWorldX = s_originalWorldPos.x + deltaFBX;
                float newWorldY = s_originalWorldPos.y + deltaFBY;
                rectTransform.x = newWorldX - parentWorldPos.x;
                rectTransform.y = newWorldY - parentWorldPos.y;
                rectTransform.width = s_originalTransform.width - deltaFBX;
                rectTransform.height = s_originalTransform.height - deltaFBY;
            }
            break;
            case 1: // Top-right
            {
                float newWorldY = s_originalWorldPos.y + deltaFBY;
                rectTransform.y = newWorldY - parentWorldPos.y;
                rectTransform.width = s_originalTransform.width + deltaFBX;
                rectTransform.height = s_originalTransform.height - deltaFBY;
            }
            break;
            case 2: // Bottom-right
                rectTransform.width = s_originalTransform.width + deltaFBX;
                rectTransform.height = s_originalTransform.height + deltaFBY;
                break;
            case 3: // Bottom-left
            {
                float newWorldX = s_originalWorldPos.x + deltaFBX;
                rectTransform.x = newWorldX - parentWorldPos.x;
                rectTransform.width = s_originalTransform.width - deltaFBX;
                rectTransform.height = s_originalTransform.height + deltaFBY;
            }
            break;
            }

            rectTransform.width = std::max(1.0f, rectTransform.width);
            rectTransform.height = std::max(1.0f, rectTransform.height);
        }

        if (s_draggingCorner >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingCorner = -1;
        }

        // Perform edge resize
        if (s_draggingEdge >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);
            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            ImVec2 parentWorldPos(0.0f, 0.0f);
            if (rectTransform.parent != std::numeric_limits<uint32_t>::max() &&
                NE::ECS::Query::HasUIRectTransform(rectTransform.parent)) {
                parentWorldPos = CalculateUIWorldPosition(rectTransform.parent);
            }

            switch (s_draggingEdge) {
            case 0: // Top edge
            {
                float newWorldY = s_originalWorldPos.y + deltaFBY;
                rectTransform.y = newWorldY - parentWorldPos.y;
                rectTransform.height = s_originalTransform.height - deltaFBY;
            }
            break;
            case 1: // Right edge
                rectTransform.width = s_originalTransform.width + deltaFBX;
                break;
            case 2: // Bottom edge
                rectTransform.height = s_originalTransform.height + deltaFBY;
                break;
            case 3: // Left edge
            {
                float newWorldX = s_originalWorldPos.x + deltaFBX;
                rectTransform.x = newWorldX - parentWorldPos.x;
                rectTransform.width = s_originalTransform.width - deltaFBX;
            }
            break;
            }

            rectTransform.width = std::max(1.0f, rectTransform.width);
            rectTransform.height = std::max(1.0f, rectTransform.height);
        }

        if (s_draggingEdge >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingEdge = -1;
        }

        // Reset on mouse release
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (s_isDraggingUI || s_draggingCorner >= 0 || s_draggingEdge >= 0)
            {
                s_isDraggingUI = false;
                s_draggingCorner = -1;
                s_draggingEdge = -1;
            }
        }
    }

    void UIGizmoHandler::End2DGizmo(uint32_t uiEntityId) {
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        s_gizmoActive = false;
        s_gizmoType = 0;
        s_gizmoEntityId = 0;
        s_isDraggingUI = false;
        s_draggingCorner = -1;
        s_draggingEdge = -1;
    }

} // namespace Editor