#include "UIGizmoHandler.hpp"
#include "imgui/imgui_internal.h"
#include "EditorInterface/ECSExports.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
#include <iostream>

namespace Editor {

    // ============ Static Variable Definitions ============
    bool UIGizmoHandler::s_gizmoActive = false;
    ImGuizmo::OPERATION UIGizmoHandler::s_currentOperation = ImGuizmo::TRANSLATE;
    uint32_t UIGizmoHandler::s_gizmoEntityId = 0;
    int UIGizmoHandler::s_gizmoType = 0;

    NE::Math::Mat4 UIGizmoHandler::s_gizmo3DStartMatrix;

    bool UIGizmoHandler::s_isDraggingUI = false;
    int UIGizmoHandler::s_draggingCorner = -1;
    int UIGizmoHandler::s_draggingEdge = -1;
    ImVec2 UIGizmoHandler::s_dragStart;
    NE::ECS::Component::UIRectTransform UIGizmoHandler::s_originalTransform;
    ImVec2 UIGizmoHandler::s_originalWorldPos;

    // Rotation state
    bool UIGizmoHandler::s_isDraggingRotation = false;
    float UIGizmoHandler::s_rotationStartAngle = 0.0f;
    float UIGizmoHandler::s_originalRotation = 0.0f;
    ImVec2 UIGizmoHandler::s_rotationCenter = ImVec2(0, 0);

    // ============ Helper Functions ============

    //static ImVec2 CalculateUIWorldPosition(uint32_t entity) {
    //    auto& rect = NE::ECS::Query::GetUIRectTransform(entity);

    //    float worldX = rect.x;
    //    float worldY = rect.y;

    //    uint32_t currentParent = rect.parent;
    //    while (currentParent != std::numeric_limits<uint32_t>::max()) {
    //        if (!NE::ECS::Query::HasUIRectTransform(currentParent)) {
    //            break;
    //        }

    //        auto& parentRect = NE::ECS::Query::GetUIRectTransform(currentParent);
    //        worldX += parentRect.x;
    //        worldY += parentRect.y;

    //        currentParent = parentRect.parent;
    //    }

    //    return ImVec2(worldX, worldY);
    //}

    static NE::Math::Mat4 BuildUIMatrix(const NE::ECS::Component::UIRectTransform& rect) {
        NE::Math::Mat4 matrix;
        matrix.SetToIdentity();

        matrix = NE::Math::Mat4::BuildTranslation(rect.GetPosition()) * matrix;
        NE::Math::Mat4 rotMatrix = rect.GetRotationMatrix();
        matrix = rotMatrix * matrix;
        NE::Math::Vec3 scale = rect.GetScale();
        NE::Math::Mat4 scaleMatrix = NE::Math::Mat4::BuildScaling(scale.x, scale.y, scale.z);
        matrix = scaleMatrix * matrix;

        return matrix;
    }

    float UIGizmoHandler::GetAngleFromCenter(ImVec2 center, ImVec2 point) {
        return std::atan2(point.y - center.y, point.x - center.x) * 180.0f / 3.14159265358979f;
    }

    // ============ 3D Gizmo (World Space UI) ============

    void UIGizmoHandler::Begin3DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize) {
        if (s_gizmoActive) return;

        auto& rect = NE::ECS::Query::GetUIRectTransform(uiEntityId);

        s_gizmo3DStartMatrix = BuildUIMatrix(rect);
        s_gizmoEntityId = uiEntityId;
        s_gizmoActive = true;
        s_gizmoType = 2;

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

        bool editedThisFrame = ImGuizmo::Manipulate(
            nullptr,
            nullptr,
            s_currentOperation,
            ImGuizmo::LOCAL,
            matrix
        );

        if (editedThisFrame && ImGuizmo::IsUsing()) {
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

    // ============ 2D Gizmo (Screen Space UI) ============

    void UIGizmoHandler::Begin2DGizmo(uint32_t uiEntityId) {
        if (s_gizmoActive) return;

        s_gizmoEntityId = uiEntityId;
        s_gizmoActive = true;
        s_gizmoType = 1;

        s_isDraggingUI = false;
        s_draggingCorner = -1;
        s_draggingEdge = -1;
        s_isDraggingRotation = false;
    }

    void UIGizmoHandler::Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
        float fbWidth, float fbHeight) {
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        auto& rectTransform = NE::ECS::Command::GetUIRectTransform(uiEntityId);

        float scaleX = panelSize.x / fbWidth;
        float scaleY = panelSize.y / fbHeight;

        // Calculate world pivot position (accumulate parent positions)
        float worldPivotX = rectTransform.x;
        float worldPivotY = rectTransform.y;

        uint32_t currentParent = rectTransform.parent;
        while (currentParent != std::numeric_limits<uint32_t>::max()) {
            if (!NE::ECS::Query::HasUIRectTransform(currentParent)) break;
            auto& parentRect = NE::ECS::Query::GetUIRectTransform(currentParent);
            worldPivotX += parentRect.x;
            worldPivotY += parentRect.y;
            currentParent = parentRect.parent;
        }

        // Calculate top-left from pivot
        float topLeftX = worldPivotX - rectTransform.width * rectTransform.pivotX;
        float topLeftY = worldPivotY - rectTransform.height * rectTransform.pivotY;

        // Convert to screen coordinates
        ImVec2 topLeft(
            panelPos.x + topLeftX * scaleX,
            panelPos.y + topLeftY * scaleY
        );
        ImVec2 bottomRight(
            panelPos.x + (topLeftX + rectTransform.width) * scaleX,
            panelPos.y + (topLeftY + rectTransform.height) * scaleY
        );
        ImVec2 center(
            panelPos.x + worldPivotX * scaleX,
            panelPos.y + worldPivotY * scaleY
        );

        // Get rotation
        float rotationZ = rectTransform.rotationZ;
        bool hasRotation = std::abs(rotationZ) > 0.001f;
        const float PI = 3.14159265358979f;
        float radians = rotationZ * PI / 180.0f;
        float cosR = std::cos(radians);
        float sinR = std::sin(radians);

        // Calculate corners (rotated around pivot/center)
        ImVec2 corners[4] = {
            topLeft,
            ImVec2(bottomRight.x, topLeft.y),
            bottomRight,
            ImVec2(topLeft.x, bottomRight.y)
        };

        // Rotate corners around the pivot point (center)
        if (hasRotation) {
            for (int i = 0; i < 4; i++) {
                float localX = corners[i].x - center.x;
                float localY = corners[i].y - center.y;
                corners[i].x = center.x + localX * cosR - localY * sinR;
                corners[i].y = center.y + localX * sinR + localY * cosR;
            }
        }

        // Calculate edge midpoints
        ImVec2 edges[4] = {
            ImVec2((corners[0].x + corners[1].x) * 0.5f, (corners[0].y + corners[1].y) * 0.5f), // Top
            ImVec2((corners[1].x + corners[2].x) * 0.5f, (corners[1].y + corners[2].y) * 0.5f), // Right
            ImVec2((corners[2].x + corners[3].x) * 0.5f, (corners[2].y + corners[3].y) * 0.5f), // Bottom
            ImVec2((corners[3].x + corners[0].x) * 0.5f, (corners[3].y + corners[0].y) * 0.5f)  // Left
        };

        const float handleSize = 8.0f;
        ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        auto mouseInPanel = [&](ImVec2 p) {
            return p.x >= panelPos.x && p.x <= panelPos.x + panelSize.x &&
                p.y >= panelPos.y && p.y <= panelPos.y + panelSize.y;
            };

        bool mouseInThisPanel = mouseInPanel(mousePos);

        // ========== DRAW GIZMO ==========

        // Draw rectangle outline
        if (hasRotation) {
            drawList->AddLine(corners[0], corners[1], IM_COL32(255, 255, 255, 255), 2.0f);
            drawList->AddLine(corners[1], corners[2], IM_COL32(255, 255, 255, 255), 2.0f);
            drawList->AddLine(corners[2], corners[3], IM_COL32(255, 255, 255, 255), 2.0f);
            drawList->AddLine(corners[3], corners[0], IM_COL32(255, 255, 255, 255), 2.0f);
        }
        else {
            drawList->AddRect(topLeft, bottomRight, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
        }

        // Draw corner handles
        for (int i = 0; i < 4; i++) {
            drawList->AddRectFilled(
                ImVec2(corners[i].x - handleSize * 0.5f, corners[i].y - handleSize * 0.5f),
                ImVec2(corners[i].x + handleSize * 0.5f, corners[i].y + handleSize * 0.5f),
                IM_COL32(0, 120, 255, 255)
            );
        }

        // Draw edge handles
        for (int i = 0; i < 4; i++) {
            drawList->AddRectFilled(
                ImVec2(edges[i].x - handleSize * 0.5f, edges[i].y - handleSize * 0.5f),
                ImVec2(edges[i].x + handleSize * 0.5f, edges[i].y + handleSize * 0.5f),
                IM_COL32(0, 120, 255, 255)
            );
        }

        // Draw center/pivot handle
        drawList->AddCircleFilled(center, handleSize * 0.5f, IM_COL32(255, 120, 0, 255)); // Orange for pivot

        // ========== ROTATION HANDLE ==========
        float rotationHandleOffset = 35.0f;
        ImVec2 rotationHandleBase = edges[0];
        ImVec2 rotationHandle;

        if (hasRotation) {
            float dirX = -sinR;
            float dirY = -cosR;
            rotationHandle.x = rotationHandleBase.x + dirX * rotationHandleOffset;
            rotationHandle.y = rotationHandleBase.y + dirY * rotationHandleOffset;
        }
        else {
            rotationHandle.x = center.x;
            rotationHandle.y = topLeft.y - rotationHandleOffset;
        }

        drawList->AddLine(rotationHandleBase, rotationHandle, IM_COL32(255, 255, 255, 180), 1.5f);

        const float rotationHandleRadius = 10.0f;
        bool hoveringRotation = false;
        {
            float dx = mousePos.x - rotationHandle.x;
            float dy = mousePos.y - rotationHandle.y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= rotationHandleRadius * rotationHandleRadius) {
                hoveringRotation = true;
            }
        }

        ImU32 rotationColor = hoveringRotation ? IM_COL32(0, 255, 100, 255) : IM_COL32(0, 200, 200, 255);
        if (s_isDraggingRotation) {
            rotationColor = IM_COL32(255, 200, 0, 255);
        }
        drawList->AddCircleFilled(rotationHandle, rotationHandleRadius, rotationColor);
        drawList->AddCircle(rotationHandle, rotationHandleRadius, IM_COL32(255, 255, 255, 255), 12, 2.0f);

        if (!s_isDraggingRotation) {
            float iconRadius = rotationHandleRadius * 0.5f;
            drawList->AddCircle(rotationHandle, iconRadius, IM_COL32(255, 255, 255, 200), 8, 1.5f);
        }

        // ========== HOVERING DETECTION ==========
        if (!s_isDraggingUI && s_draggingCorner < 0 && s_draggingEdge < 0 && !s_isDraggingRotation && mouseInThisPanel)
        {
            bool hoveringHandle = false;

            if (hoveringRotation) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                hoveringHandle = true;
            }

            if (!hoveringHandle) {
                for (int i = 0; i < 4; ++i)
                {
                    float dx = mousePos.x - corners[i].x;
                    float dy = mousePos.y - corners[i].y;
                    float dist2 = dx * dx + dy * dy;

                    if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                        hoveringHandle = true;
                        break;
                    }
                }
            }

            if (!hoveringHandle)
            {
                for (int i = 0; i < 4; ++i)
                {
                    float dx = mousePos.x - edges[i].x;
                    float dy = mousePos.y - edges[i].y;
                    float dist2 = dx * dx + dy * dy;

                    if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                    {
                        if (i == 0 || i == 2) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        else ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        hoveringHandle = true;
                        break;
                    }
                }
            }

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

        // ========== HANDLE CLICKING ==========
        if (!s_isDraggingUI && s_draggingCorner < 0 && s_draggingEdge < 0 && !s_isDraggingRotation &&
            mouseInThisPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            bool handleClicked = false;

            if (hoveringRotation)
            {
                s_isDraggingRotation = true;
                s_rotationCenter = center;
                s_rotationStartAngle = GetAngleFromCenter(center, mousePos);
                s_originalRotation = rectTransform.rotationZ;
                handleClicked = true;
            }

            if (!handleClicked) {
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
                        handleClicked = true;
                        break;
                    }
                }
            }

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
                        handleClicked = true;
                        break;
                    }
                }
            }

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
                }
            }
        }

        // ========== ROTATION DRAG ==========
        if (s_isDraggingRotation && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float currentAngle = GetAngleFromCenter(s_rotationCenter, mousePos);
            float deltaAngle = currentAngle - s_rotationStartAngle;

            rectTransform.rotationZ = s_originalRotation + deltaAngle;

            while (rectTransform.rotationZ > 180.0f) rectTransform.rotationZ -= 360.0f;
            while (rectTransform.rotationZ < -180.0f) rectTransform.rotationZ += 360.0f;

            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
                rectTransform.rotationZ = std::round(rectTransform.rotationZ / 15.0f) * 15.0f;
            }

            if (std::abs(rectTransform.rotationZ) < 3.0f &&
                !(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift))) {
                rectTransform.rotationZ = 0.0f;
            }
        }

        if (s_isDraggingRotation && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingRotation = false;
        }

        // ========== POSITION DRAG (pivot moves) ==========
        if (s_isDraggingUI && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);
            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            rectTransform.x = s_originalTransform.x + deltaFBX;
            rectTransform.y = s_originalTransform.y + deltaFBY;
        }

        if (s_isDraggingUI && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingUI = false;
        }

        // ========== CORNER RESIZE (pivot stays fixed!) ==========
        if (s_draggingCorner >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);

            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            float origRotation = s_originalTransform.rotationZ;
            float origRadians = origRotation * PI / 180.0f;
            float origCosR = std::cos(origRadians);
            float origSinR = std::sin(origRadians);

            // Transform to local space
            float localDeltaX = deltaFBX * origCosR + deltaFBY * origSinR;
            float localDeltaY = -deltaFBX * origSinR + deltaFBY * origCosR;

            float newWidth = s_originalTransform.width;
            float newHeight = s_originalTransform.height;

            // For pivot-based system, resizing just changes width/height
            // The pivot position stays the same!
            switch (s_draggingCorner) {
            case 0: // Top-left
                newWidth = s_originalTransform.width - localDeltaX;
                newHeight = s_originalTransform.height - localDeltaY;
                break;
            case 1: // Top-right
                newWidth = s_originalTransform.width + localDeltaX;
                newHeight = s_originalTransform.height - localDeltaY;
                break;
            case 2: // Bottom-right
                newWidth = s_originalTransform.width + localDeltaX;
                newHeight = s_originalTransform.height + localDeltaY;
                break;
            case 3: // Bottom-left
                newWidth = s_originalTransform.width - localDeltaX;
                newHeight = s_originalTransform.height + localDeltaY;
                break;
            }

            rectTransform.width = std::max(1.0f, newWidth);
            rectTransform.height = std::max(1.0f, newHeight);
            // Position (pivot) stays the same!
        }

        if (s_draggingCorner >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingCorner = -1;
        }

        // ========== EDGE RESIZE (pivot stays fixed!) ==========
        if (s_draggingEdge >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);

            float deltaFBX = deltaPixels.x / scaleX;
            float deltaFBY = deltaPixels.y / scaleY;

            float origRotation = s_originalTransform.rotationZ;
            float origRadians = origRotation * PI / 180.0f;
            float origCosR = std::cos(origRadians);
            float origSinR = std::sin(origRadians);

            // Transform to local space
            float localDeltaX = deltaFBX * origCosR + deltaFBY * origSinR;
            float localDeltaY = -deltaFBX * origSinR + deltaFBY * origCosR;

            float newWidth = s_originalTransform.width;
            float newHeight = s_originalTransform.height;

            switch (s_draggingEdge) {
            case 0: // Top edge
                newHeight = s_originalTransform.height - localDeltaY;
                break;
            case 1: // Right edge
                newWidth = s_originalTransform.width + localDeltaX;
                break;
            case 2: // Bottom edge
                newHeight = s_originalTransform.height + localDeltaY;
                break;
            case 3: // Left edge
                newWidth = s_originalTransform.width - localDeltaX;
                break;
            }

            rectTransform.width = std::max(1.0f, newWidth);
            rectTransform.height = std::max(1.0f, newHeight);
            // Position (pivot) stays the same!
        }

        if (s_draggingEdge >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingEdge = -1;
        }

        // ========== CLEANUP ==========
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (s_isDraggingUI || s_draggingCorner >= 0 || s_draggingEdge >= 0 || s_isDraggingRotation)
            {
                s_isDraggingUI = false;
                s_draggingCorner = -1;
                s_draggingEdge = -1;
                s_isDraggingRotation = false;
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
        s_isDraggingRotation = false;
    }

} // namespace Editor