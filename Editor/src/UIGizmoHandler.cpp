#include "UIGizmoHandler.hpp"
#include "imgui/imgui_internal.h"
#include "EditorInterface/ECSExports.hpp"
#include "Command/CommandHistory.hpp"
#include <ECS/Components/UICanvas.hpp>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
#include <iostream>
#include <vector>

namespace Editor {

    // ============ Static Variable Definitions ============
    bool UIGizmoHandler::s_gizmoActive = false;
    ImGuizmo::OPERATION UIGizmoHandler::s_currentOperation = ImGuizmo::TRANSLATE;
    uint32_t UIGizmoHandler::s_gizmoEntityId = 0;
    int UIGizmoHandler::s_gizmoType = 0;

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
    constexpr float ROTATION_SENSITIVITY = 5.0f;

    // Undo/Redo command state
    std::unique_ptr<SetUIRectTransformCommand> UIGizmoHandler::s_uiGizmoCmd = nullptr;
    uint8_t UIGizmoHandler::s_uiGizmoMask = 0;

    // ============ Helper Functions ============

    NE::Math::Mat4 UIGizmoHandler::BuildUIWorldTRS(uint32_t entityId)
    {
        using namespace NE::ECS;
        using namespace NE::ECS::Component;

        NE::Math::Mat4 identity;
        identity.SetToIdentity();

        if (!Query::HasUIRectTransform(entityId))
            return identity;

        auto& rect = Query::GetUIRectTransform(entityId);

        // Find canvas entity
        uint32_t canvasEntityId = std::numeric_limits<uint32_t>::max();
        const UICanvas* canvas = nullptr;

        // First check if this entity itself is a canvas
        if (Query::HasUICanvas(entityId)) {
            canvasEntityId = entityId;
            canvas = &Query::GetUICanvas(entityId);
        } else {
            // Walk up parent chain to find canvas
            uint32_t currentParent = rect.parent;
            while (currentParent != std::numeric_limits<uint32_t>::max()) {
                if (Query::HasUICanvas(currentParent)) {
                    canvasEntityId = currentParent;
                    canvas = &Query::GetUICanvas(currentParent);
                    break;
                }
                if (!Query::HasUIRectTransform(currentParent)) break;
                currentParent = Query::GetUIRectTransform(currentParent).parent;
            }
        }

        // Only build world matrix for world space UI
        if (!canvas || canvas->renderMode != UICanvas::RenderMode::WORLD_SPACE) {
            return identity;
        }

        constexpr float PI = 3.14159265358979f;

        // Build parent chain (including canvas for world space)
        std::vector<uint32_t> chain;
        uint32_t current = entityId;

        while (current != std::numeric_limits<uint32_t>::max() && Query::HasUIRectTransform(current)) {
            chain.push_back(current);
            if (current == canvasEntityId) break; // Include canvas for world space
            current = Query::GetUIRectTransform(current).parent;
        }

        std::reverse(chain.begin(), chain.end());

        // Accumulate transforms along the chain
        NE::Math::Vec3 accumulatedPos(0, 0, 0);
        NE::Math::Vec3 accumulatedScale(1, 1, 1);
        float accumulatedRotX = 0.0f;
        float accumulatedRotY = 0.0f;
        float accumulatedRotZ = 0.0f;

        for (uint32_t entity : chain) {
            auto& currentRect = Query::GetUIRectTransform(entity);

            // Accumulate position
            accumulatedPos.x += currentRect.x;
            accumulatedPos.y += currentRect.y;
            accumulatedPos.z += currentRect.z;

            // Accumulate scale
            accumulatedScale.x *= currentRect.scaleX;
            accumulatedScale.y *= currentRect.scaleY;
            accumulatedScale.z *= currentRect.scaleZ;

            // Accumulate rotation
            accumulatedRotX += currentRect.rotationX;
            accumulatedRotY += currentRect.rotationY;
            accumulatedRotZ += currentRect.rotationZ;
        }

        // Build TRS matrix - match TransformSystem's order: T * R * S
        // Rotation order: X * Y * Z (same as TransformSystem)
        NE::Math::Mat4 T = NE::Math::Mat4::BuildTranslation(
            accumulatedPos.x,
            accumulatedPos.y,
            accumulatedPos.z
        );

        NE::Math::Mat4 Rx = NE::Math::Mat4::BuildXRotation(accumulatedRotX * PI / 180.0f);
        NE::Math::Mat4 Ry = NE::Math::Mat4::BuildYRotation(accumulatedRotY * PI / 180.0f);
        NE::Math::Mat4 Rz = NE::Math::Mat4::BuildZRotation(accumulatedRotZ * PI / 180.0f);
        NE::Math::Mat4 R = Rx * Ry * Rz; // X * Y * Z order (same as TransformSystem)

        NE::Math::Mat4 S = NE::Math::Mat4::BuildScaling(
            accumulatedScale.x,
            accumulatedScale.y,
            accumulatedScale.z
        );

        // TRS order: T * R * S (same as TransformSystem)
        return T * R * S;
    }

    float UIGizmoHandler::GetAngleFromCenter(ImVec2 center, ImVec2 point) {
        return std::atan2(point.y - center.y, point.x - center.x) * 180.0f / 3.14159265358979f;
    }

    void UIGizmoHandler::CommitCommand() {
        if (!s_uiGizmoCmd) return;

        const auto& before = s_uiGizmoCmd->Before();
        const auto& after = s_uiGizmoCmd->After();

        bool changed = false;

        if (s_uiGizmoMask & SetUIRectTransformCommand::Pos) {
            changed |= (std::fabs(before.x - after.x) > 1e-6f ||
                std::fabs(before.y - after.y) > 1e-6f ||
                std::fabs(before.z - after.z) > 1e-6f);
        }
        if (s_uiGizmoMask & SetUIRectTransformCommand::Rot) {
            changed |= (std::fabs(before.rotationX - after.rotationX) > 1e-6f ||
                std::fabs(before.rotationY - after.rotationY) > 1e-6f ||
                std::fabs(before.rotationZ - after.rotationZ) > 1e-6f);
        }
        if (s_uiGizmoMask & SetUIRectTransformCommand::Scl) {
            changed |= (std::fabs(before.scaleX - after.scaleX) > 1e-6f ||
                std::fabs(before.scaleY - after.scaleY) > 1e-6f ||
                std::fabs(before.scaleZ - after.scaleZ) > 1e-6f);
        }
        if (s_uiGizmoMask & SetUIRectTransformCommand::Size) {
            changed |= (std::fabs(before.width - after.width) > 1e-6f ||
                std::fabs(before.height - after.height) > 1e-6f);
        }
        if (s_uiGizmoMask & SetUIRectTransformCommand::Pivot) {
            changed |= (std::fabs(before.pivotX - after.pivotX) > 1e-6f ||
                std::fabs(before.pivotY - after.pivotY) > 1e-6f);
        }

        if (changed) {
            CommandHistory::GetInstance().ExecuteCommand(std::move(s_uiGizmoCmd));
        }
        else {
            s_uiGizmoCmd.reset();
        }

        s_uiGizmoMask = 0;
    }

    void UIGizmoHandler::UpdateCommandAfter(const NE::ECS::Component::UIRectTransform& rect) {
        if (s_uiGizmoCmd) {
            s_uiGizmoCmd->SetAfter(rect);
        }
    }

    // ============ 3D Gizmo (World Space UI) ============

    void UIGizmoHandler::Begin3DGizmo(uint32_t uiEntityId, ImVec2 /*panelPos*/, ImVec2 /*panelSize*/)
    {
        if (s_gizmoActive) return;

        auto& rect = NE::ECS::Query::GetUIRectTransform(uiEntityId);

        s_gizmoEntityId = uiEntityId;
        s_gizmoActive = true;
        s_gizmoType = 2;
        s_originalTransform = rect;

        // Create command based on current operation
        switch (s_currentOperation) {
        case ImGuizmo::TRANSLATE: s_uiGizmoMask = SetUIRectTransformCommand::Pos; break;
        case ImGuizmo::ROTATE:    s_uiGizmoMask = SetUIRectTransformCommand::Rot; break;
        case ImGuizmo::SCALE:     s_uiGizmoMask = SetUIRectTransformCommand::Scl; break;
        default:                  s_uiGizmoMask = SetUIRectTransformCommand::All; break;
        }

        s_uiGizmoCmd = std::make_unique<SetUIRectTransformCommand>(
            uiEntityId, "UI Gizmo: Transform",
            s_originalTransform, s_originalTransform,
            &NE::ECS::Command::GetUIRectTransform,
            s_uiGizmoMask
        );
    }

    void UIGizmoHandler::Update3DGizmo(uint32_t uiEntityId,
        const NE::Math::Mat4& view,
        const NE::Math::Mat4& proj,
        ImVec2 panelPos,
        ImVec2 panelSize)
    {
        // Set up ImGuizmo for this panel
        ImGuizmo::BeginFrame();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(panelPos.x, panelPos.y, panelSize.x, panelSize.y);

        // Build WORLD TRS (no width/pivot)
        NE::Math::Mat4 worldMatrix = BuildUIWorldTRS(uiEntityId);

        float matrix[16];
        memcpy(matrix, worldMatrix.Data(), sizeof(float) * 16);

        bool editedThisFrame = ImGuizmo::Manipulate(
            view.Data(),
            proj.Data(),
            s_currentOperation,
            ImGuizmo::LOCAL,
            matrix
        );

        bool isUsing = ImGuizmo::IsUsing();

        // Begin tracking when user starts dragging
        if (!s_gizmoActive && isUsing) {
            Begin3DGizmo(uiEntityId, panelPos, panelSize);
        }

        // Update transform while dragging
        if (s_gizmoActive && isUsing && editedThisFrame) {
            auto& rectCmd = NE::ECS::Command::GetUIRectTransform(uiEntityId);

            // ========== DO IT LIKE 3D ENTITIES! ==========

            // 1. Get new world matrix from ImGuizmo
            NE::Math::Mat4 newWorld;
            memcpy(newWorld.Data(), matrix, sizeof(float) * 16);

            // 2. Find canvas entity
            uint32_t canvasEntityId = std::numeric_limits<uint32_t>::max();
            if (NE::ECS::Query::HasUICanvas(uiEntityId)) {
                canvasEntityId = uiEntityId;
            } else {
                uint32_t p = rectCmd.parent;
                while (p != std::numeric_limits<uint32_t>::max()) {
                    if (NE::ECS::Query::HasUICanvas(p)) {
                        canvasEntityId = p;
                        break;
                    }
                    if (!NE::ECS::Query::HasUIRectTransform(p)) break;
                    p = NE::ECS::Query::GetUIRectTransform(p).parent;
                }
            }

            // 3. Build parent world matrix (all parents up to and including canvas for world space)
            // Build parent chain first (from immediate parent to canvas)
            std::vector<uint32_t> parentChain;
            uint32_t p = rectCmd.parent;
            while (p != std::numeric_limits<uint32_t>::max() &&
                NE::ECS::Query::HasUIRectTransform(p))
            {
                parentChain.push_back(p);
                // Stop at canvas for world space (canvas transform is included)
                if (p == canvasEntityId) break;
                p = NE::ECS::Query::GetUIRectTransform(p).parent;
            }

            // Reverse to get root-to-leaf order (canvas first, then immediate parent)
            std::reverse(parentChain.begin(), parentChain.end());

            // Build parent world matrix from root to leaf
            NE::Math::Mat4 parentWorld;
            parentWorld.SetToIdentity();

            constexpr float PI = 3.14159265358979f;
            for (uint32_t parentId : parentChain) {
                auto& parentRect = NE::ECS::Query::GetUIRectTransform(parentId);

                // Build parent's TRS - match TransformSystem's order: T * R * S
                // Rotation order: X * Y * Z (same as TransformSystem)
                NE::Math::Mat4 pT = NE::Math::Mat4::BuildTranslation(parentRect.x, parentRect.y, parentRect.z);
                NE::Math::Mat4 pRx = NE::Math::Mat4::BuildXRotation(parentRect.rotationX * PI / 180.0f);
                NE::Math::Mat4 pRy = NE::Math::Mat4::BuildYRotation(parentRect.rotationY * PI / 180.0f);
                NE::Math::Mat4 pRz = NE::Math::Mat4::BuildZRotation(parentRect.rotationZ * PI / 180.0f);
                NE::Math::Mat4 pR = pRx * pRy * pRz; // X * Y * Z order (same as TransformSystem)
                NE::Math::Mat4 pS = NE::Math::Mat4::BuildScaling(parentRect.scaleX, parentRect.scaleY, parentRect.scaleZ);

                // Accumulate from root to leaf: parentWorld = parentWorld * (pT * pR * pS)
                parentWorld = parentWorld * (pT * pR * pS);
            }

            // 4. Convert to local matrix: local = parent^-1 * world
            NE::Math::Mat4 invParent = parentWorld.Inverse();
            NE::Math::Mat4 newLocal = invParent * newWorld;

            // 5. Decompose LOCAL matrix using ImGuizmo (same as 3D entities!)
            float localMatrix[16];
            memcpy(localMatrix, newLocal.Data(), sizeof(float) * 16);

            float tr[3], rotDeg[3], sc[3];
            ImGuizmo::DecomposeMatrixToComponents(localMatrix, tr, rotDeg, sc);

            // 6. Validate and clamp decomposed values to prevent NaN/Inf
            constexpr float MIN_SCALE = 0.001f;
            constexpr float MAX_SCALE = 1000.0f;
            constexpr float MAX_POS = 100000.0f;

            // Clamp translation
            tr[0] = std::clamp(tr[0], -MAX_POS, MAX_POS);
            tr[1] = std::clamp(tr[1], -MAX_POS, MAX_POS);
            tr[2] = std::clamp(tr[2], -MAX_POS, MAX_POS);

            // Normalize rotation to -180 to 180 range
            for (int i = 0; i < 3; i++) {
                while (rotDeg[i] > 180.0f) rotDeg[i] -= 360.0f;
                while (rotDeg[i] < -180.0f) rotDeg[i] += 360.0f;
            }

            // Clamp scale (prevent zero or negative)
            for (int i = 0; i < 3; i++) {
                if (std::isnan(sc[i]) || std::isinf(sc[i]) || sc[i] < MIN_SCALE) {
                    sc[i] = MIN_SCALE;
                }
                sc[i] = std::clamp(sc[i], MIN_SCALE, MAX_SCALE);
            }

            // 7. Save local values (only update what the current operation affects)
            // Note: ImGuizmo returns rotation in degrees, which matches our storage
            if (s_currentOperation == ImGuizmo::TRANSLATE || s_currentOperation == ImGuizmo::UNIVERSAL) {
                rectCmd.x = tr[0];
                rectCmd.y = tr[1];
                rectCmd.z = tr[2];
            }

            if (s_currentOperation == ImGuizmo::ROTATE || s_currentOperation == ImGuizmo::UNIVERSAL) {
                rectCmd.rotationX = rotDeg[0];
                rectCmd.rotationY = rotDeg[1];
                rectCmd.rotationZ = rotDeg[2];
            }

            if (s_currentOperation == ImGuizmo::SCALE || s_currentOperation == ImGuizmo::UNIVERSAL) {
                rectCmd.scaleX = sc[0];
                rectCmd.scaleY = sc[1];
                rectCmd.scaleZ = sc[2];
            }

            // =============================================

            // Update command for undo/redo
            UpdateCommandAfter(rectCmd);
        }

        // End tracking when user releases
        if (s_gizmoActive && !isUsing) {
            End3DGizmo(uiEntityId);
        }
    }

    void UIGizmoHandler::End3DGizmo(uint32_t uiEntityId) {
        if (!s_gizmoActive || s_gizmoType != 2 || s_gizmoEntityId != uiEntityId) return;

        // Commit command
        CommitCommand();

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

        // Reset command state
        s_uiGizmoCmd.reset();
        s_uiGizmoMask = 0;
    }

    void UIGizmoHandler::Update2DGizmo(uint32_t uiEntityId, ImVec2 panelPos, ImVec2 panelSize,
        float fbWidth, float fbHeight) {
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        auto& rectTransform = NE::ECS::Command::GetUIRectTransform(uiEntityId);

        float panelScaleX = panelSize.x / fbWidth;
        float panelScaleY = panelSize.y / fbHeight;

        // Use UITransformSystem to get world transform (accounts for anchors properly)
        auto worldTransform = NE::ECS::Query::GetUIWorldTransform(uiEntityId);
        
        // worldTransform.x/y is the pivot position in top-left origin coordinates
        float worldPivotX = worldTransform.x;
        float worldPivotY = worldTransform.y;
        
        // worldTransform.width/height are already scaled
        float scaledWidth = worldTransform.width;
        float scaledHeight = worldTransform.height;

        // Calculate top-left from pivot (Unity-style: pivot (0,0) = bottom-left)
        float topLeftX = worldPivotX - scaledWidth * rectTransform.pivotX;
        float topLeftY = worldPivotY - scaledHeight * (1.0f - rectTransform.pivotY);

        // Convert to screen coordinates
        ImVec2 topLeft(
            panelPos.x + topLeftX * panelScaleX,
            panelPos.y + topLeftY * panelScaleY
        );
        ImVec2 bottomRight(
            panelPos.x + (topLeftX + scaledWidth) * panelScaleX,
            panelPos.y + (topLeftY + scaledHeight) * panelScaleY
        );
        ImVec2 center(
            panelPos.x + worldPivotX * panelScaleX,
            panelPos.y + worldPivotY * panelScaleY
        );

        // Get rotation (use accumulated rotation from world transform)
        float rotationZ = worldTransform.accumulatedRotationZ;
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
        drawList->AddCircleFilled(center, handleSize * 0.5f, IM_COL32(255, 120, 0, 255));

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

            // Rotation handle
            if (hoveringRotation)
            {
                s_isDraggingRotation = true;
                s_rotationCenter = center;
                s_rotationStartAngle = GetAngleFromCenter(center, mousePos);
                s_originalRotation = rectTransform.rotationZ;
                s_originalTransform = rectTransform;
                handleClicked = true;

                s_uiGizmoMask = SetUIRectTransformCommand::Rot;
                s_uiGizmoCmd = std::make_unique<SetUIRectTransformCommand>(
                    uiEntityId, "UI Gizmo: Rotate",
                    s_originalTransform, s_originalTransform,
                    &NE::ECS::Command::GetUIRectTransform,
                    s_uiGizmoMask
                );
            }

            // Corner handles
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

                        s_uiGizmoMask = SetUIRectTransformCommand::Size;
                        s_uiGizmoCmd = std::make_unique<SetUIRectTransformCommand>(
                            uiEntityId, "UI Gizmo: Resize",
                            s_originalTransform, s_originalTransform,
                            &NE::ECS::Command::GetUIRectTransform,
                            s_uiGizmoMask
                        );
                        break;
                    }
                }
            }

            // Edge handles
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

                        s_uiGizmoMask = SetUIRectTransformCommand::Size;
                        s_uiGizmoCmd = std::make_unique<SetUIRectTransformCommand>(
                            uiEntityId, "UI Gizmo: Resize",
                            s_originalTransform, s_originalTransform,
                            &NE::ECS::Command::GetUIRectTransform,
                            s_uiGizmoMask
                        );
                        break;
                    }
                }
            }

            // Center/pivot handle (position drag)
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

                    s_uiGizmoMask = SetUIRectTransformCommand::Pos;
                    s_uiGizmoCmd = std::make_unique<SetUIRectTransformCommand>(
                        uiEntityId, "UI Gizmo: Move",
                        s_originalTransform, s_originalTransform,
                        &NE::ECS::Command::GetUIRectTransform,
                        s_uiGizmoMask
                    );
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

            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }

        if (s_isDraggingRotation && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingRotation = false;
            CommitCommand();
        }

        // ========== POSITION DRAG ==========
        if (s_isDraggingUI && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);

            // Convert screen pixels to framebuffer units
            float deltaFBX = deltaPixels.x / panelScaleX;
            float deltaFBY = deltaPixels.y / panelScaleY;

            rectTransform.x = s_originalTransform.x + deltaFBX;
            rectTransform.y = s_originalTransform.y + deltaFBY;

            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }

        if (s_isDraggingUI && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingUI = false;
            CommitCommand();
        }

        // ========== CORNER RESIZE ==========
        if (s_draggingCorner >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);

            // Convert screen pixels to framebuffer units
            float deltaFBX = deltaPixels.x / panelScaleX;
            float deltaFBY = deltaPixels.y / panelScaleY;

            // Account for rotation
            float origRotation = s_originalTransform.rotationZ;
            float origRadians = origRotation * PI / 180.0f;
            float origCosR = std::cos(origRadians);
            float origSinR = std::sin(origRadians);

            // Transform to local (rotated) space
            float localDeltaX = deltaFBX * origCosR + deltaFBY * origSinR;
            float localDeltaY = -deltaFBX * origSinR + deltaFBY * origCosR;

            // Account for world scale - divide by scale to get local width/height change
            // Use the scale at drag start for consistency
            float origWorldScaleX = s_originalTransform.scaleX;
            float origWorldScaleY = s_originalTransform.scaleY;

            // Accumulate parent scale from original state
            uint32_t p = s_originalTransform.parent;
            while (p != std::numeric_limits<uint32_t>::max() && NE::ECS::Query::HasUIRectTransform(p)) {
                auto& parentRect = NE::ECS::Query::GetUIRectTransform(p);
                origWorldScaleX *= parentRect.scaleX;
                origWorldScaleY *= parentRect.scaleY;
                p = parentRect.parent;
            }

            // Convert to local units (divide by world scale)
            float localWidthDelta = (origWorldScaleX > 0.001f) ? localDeltaX / origWorldScaleX : localDeltaX;
            float localHeightDelta = (origWorldScaleY > 0.001f) ? localDeltaY / origWorldScaleY : localDeltaY;

            float newWidth = s_originalTransform.width;
            float newHeight = s_originalTransform.height;

            switch (s_draggingCorner) {
            case 0: // Top-left
                newWidth = s_originalTransform.width - localWidthDelta;
                newHeight = s_originalTransform.height - localHeightDelta;
                break;
            case 1: // Top-right
                newWidth = s_originalTransform.width + localWidthDelta;
                newHeight = s_originalTransform.height - localHeightDelta;
                break;
            case 2: // Bottom-right
                newWidth = s_originalTransform.width + localWidthDelta;
                newHeight = s_originalTransform.height + localHeightDelta;
                break;
            case 3: // Bottom-left
                newWidth = s_originalTransform.width - localWidthDelta;
                newHeight = s_originalTransform.height + localHeightDelta;
                break;
            }

            rectTransform.width = std::max(1.0f, newWidth);
            rectTransform.height = std::max(1.0f, newHeight);

            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }

        if (s_draggingCorner >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingCorner = -1;
            CommitCommand();
        }

        // ========== EDGE RESIZE ==========
        if (s_draggingEdge >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ImVec2 deltaPixels(mousePos.x - s_dragStart.x, mousePos.y - s_dragStart.y);

            // Convert screen pixels to framebuffer units
            float deltaFBX = deltaPixels.x / panelScaleX;
            float deltaFBY = deltaPixels.y / panelScaleY;

            // Account for rotation
            float origRotation = s_originalTransform.rotationZ;
            float origRadians = origRotation * PI / 180.0f;
            float origCosR = std::cos(origRadians);
            float origSinR = std::sin(origRadians);

            // Transform to local (rotated) space
            float localDeltaX = deltaFBX * origCosR + deltaFBY * origSinR;
            float localDeltaY = -deltaFBX * origSinR + deltaFBY * origCosR;

            // Account for world scale
            float origWorldScaleX = s_originalTransform.scaleX;
            float origWorldScaleY = s_originalTransform.scaleY;

            uint32_t p = s_originalTransform.parent;
            while (p != std::numeric_limits<uint32_t>::max() && NE::ECS::Query::HasUIRectTransform(p)) {
                auto& parentRect = NE::ECS::Query::GetUIRectTransform(p);
                origWorldScaleX *= parentRect.scaleX;
                origWorldScaleY *= parentRect.scaleY;
                p = parentRect.parent;
            }

            // Convert to local units
            float localWidthDelta = (origWorldScaleX > 0.001f) ? localDeltaX / origWorldScaleX : localDeltaX;
            float localHeightDelta = (origWorldScaleY > 0.001f) ? localDeltaY / origWorldScaleY : localDeltaY;

            float newWidth = s_originalTransform.width;
            float newHeight = s_originalTransform.height;

            switch (s_draggingEdge) {
            case 0: // Top edge
                newHeight = s_originalTransform.height - localHeightDelta;
                break;
            case 1: // Right edge
                newWidth = s_originalTransform.width + localWidthDelta;
                break;
            case 2: // Bottom edge
                newHeight = s_originalTransform.height + localHeightDelta;
                break;
            case 3: // Left edge
                newWidth = s_originalTransform.width - localWidthDelta;
                break;
            }

            rectTransform.width = std::max(1.0f, newWidth);
            rectTransform.height = std::max(1.0f, newHeight);

            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }

        if (s_draggingEdge >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingEdge = -1;
            CommitCommand();
        }

        // ========== CLEANUP ==========
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (s_isDraggingUI || s_draggingCorner >= 0 || s_draggingEdge >= 0 || s_isDraggingRotation)
            {
                CommitCommand();

                s_isDraggingUI = false;
                s_draggingCorner = -1;
                s_draggingEdge = -1;
                s_isDraggingRotation = false;
            }
        }
    }

    void UIGizmoHandler::End2DGizmo(uint32_t uiEntityId) {
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        // Commit any pending command
        CommitCommand();

        s_gizmoActive = false;
        s_gizmoType = 0;
        s_gizmoEntityId = 0;
        s_isDraggingUI = false;
        s_draggingCorner = -1;
        s_draggingEdge = -1;
        s_isDraggingRotation = false;
    }

} // namespace Editor