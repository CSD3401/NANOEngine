#include "UIGizmoHandler.hpp"
#include "imgui/imgui_internal.h"
#include "EditorInterface/ECSExports.hpp"
#include "Command/CommandHistory.hpp"
#include <ECS/Components/UICanvas.hpp>
#include <Math/Vec4.hpp>
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
    ImVec2 UIGizmoHandler::s_originalWorldPos;  // For 2D gizmo
    NE::Math::Vec3 UIGizmoHandler::s_originalWorldPos3D;  // For 3D world space gizmo
    float UIGizmoHandler::s_originalPivotZ3D = 0.0f;  // Store original pivot Z for 3D gizmo translation

    // Rotation state
    bool UIGizmoHandler::s_isDraggingRotation = false;
    float UIGizmoHandler::s_rotationStartAngle = 0.0f;
    float UIGizmoHandler::s_originalRotation = 0.0f;
    float UIGizmoHandler::s_cumulativeRotation = 0.0f;  // Track cumulative rotation to handle -180/180 wrapping
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

    // Helper function to project world space point to screen space
    static bool WorldToScreen(const NE::Math::Vec3& worldPos,
        const NE::Math::Mat4& view,
        const NE::Math::Mat4& proj,
        const ImVec2& panelPos,
        const ImVec2& panelSize,
        ImVec2& outScreen)
    {
        NE::Math::Mat4 VP = proj * view;
        
        // Transform to clip space (manually multiply Mat4 * Vec4, avoiding Vec4 constructor)
        float inputX = worldPos.x;
        float inputY = worldPos.y;
        float inputZ = worldPos.z;
        float inputW = 1.0f;
        
        float clipX = VP.GetElement(0, 0) * inputX + VP.GetElement(0, 1) * inputY + VP.GetElement(0, 2) * inputZ + VP.GetElement(0, 3) * inputW;
        float clipY = VP.GetElement(1, 0) * inputX + VP.GetElement(1, 1) * inputY + VP.GetElement(1, 2) * inputZ + VP.GetElement(1, 3) * inputW;
        float clipW = VP.GetElement(3, 0) * inputX + VP.GetElement(3, 1) * inputY + VP.GetElement(3, 2) * inputZ + VP.GetElement(3, 3) * inputW;
        
        if (clipW <= 1e-5f) return false; // behind camera / invalid
        
        // Perspective divide
        float invW = 1.0f / clipW;
        float ndcX = clipX * invW;
        float ndcY = clipY * invW;
        
        // Optional: quick reject if outside viewport
        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f)
            return false;
        
        // Convert NDC to screen space
        const float u = ndcX * 0.5f + 0.5f;  // [0..1]
        const float v = ndcY * 0.5f + 0.5f;  // [0..1] (Y-up)
        outScreen.x = panelPos.x + u * panelSize.x;
        outScreen.y = panelPos.y + (1.0f - v) * panelSize.y;  // Flip Y for ImGui
        return true;
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

        // Draw canvas bounds visualization (wireframe rectangle) for world space canvas
        // This helps visualize where the canvas corners are for anchoring
        if (NE::ECS::Query::HasUICanvas(uiEntityId)) {
            auto& canvasRect = NE::ECS::Query::GetUIRectTransform(uiEntityId);
            auto canvas = NE::ECS::Query::GetUICanvas(uiEntityId);
            
            if (canvas.renderMode == NE::ECS::Component::UICanvas::RenderMode::WORLD_SPACE) {
                // Calculate canvas corners in UI coordinate space (not scaled)
                // The wireframe should show the UI coordinate bounds (100x100), not the scaled world bounds
                float pivotX = canvasRect.pivotX;
                float pivotY = canvasRect.pivotY;
                float width = canvasRect.width;  // UI coordinate space width (e.g., 100)
                float height = canvasRect.height; // UI coordinate space height (e.g., 100)
                
                // Calculate corners relative to pivot in UI coordinate space (Y-down)
                // These will be transformed to world space, but we want to show the full UI coordinate bounds
                NE::Math::Vec3 cornersLocal[4] = {
                    NE::Math::Vec3(-width * pivotX, -height * (1.0f - pivotY), 0.0f),  // Top-left (Y-down)
                    NE::Math::Vec3(width * (1.0f - pivotX), -height * (1.0f - pivotY), 0.0f),  // Top-right
                    NE::Math::Vec3(width * (1.0f - pivotX), height * pivotY, 0.0f),  // Bottom-right
                    NE::Math::Vec3(-width * pivotX, height * pivotY, 0.0f)   // Bottom-left
                };
                
                // Build matrix with position and rotation, but WITHOUT scale
                // This way the wireframe shows the UI coordinate space (100x100), not the scaled size (1x1)
                constexpr float PI = 3.14159265358979f;
                NE::Math::Mat4 translationMatrix = NE::Math::Mat4::BuildTranslation(
                    canvasRect.x, canvasRect.y, canvasRect.z
                );
                NE::Math::Mat4 rotationX = NE::Math::Mat4::BuildXRotation(canvasRect.rotationX * PI / 180.0f);
                NE::Math::Mat4 rotationY = NE::Math::Mat4::BuildYRotation(canvasRect.rotationY * PI / 180.0f);
                NE::Math::Mat4 rotationZ = NE::Math::Mat4::BuildZRotation(canvasRect.rotationZ * PI / 180.0f);
                NE::Math::Mat4 rotationMatrix = rotationX * rotationY * rotationZ;
                
                // Matrix without scale: T * R (no scale)
                NE::Math::Mat4 canvasMatrix = translationMatrix * rotationMatrix;
                
                // Transform local corners to world space
                NE::Math::Vec3 cornersWorld[4];
                for (int i = 0; i < 4; ++i) {
                    float cornerX = cornersLocal[i].x;
                    float cornerY = cornersLocal[i].y;
                    float cornerZ = cornersLocal[i].z;
                    float cornerW = 1.0f;
                    
                    cornersWorld[i].x = canvasMatrix.GetElement(0, 0) * cornerX + canvasMatrix.GetElement(0, 1) * cornerY + canvasMatrix.GetElement(0, 2) * cornerZ + canvasMatrix.GetElement(0, 3) * cornerW;
                    cornersWorld[i].y = canvasMatrix.GetElement(1, 0) * cornerX + canvasMatrix.GetElement(1, 1) * cornerY + canvasMatrix.GetElement(1, 2) * cornerZ + canvasMatrix.GetElement(1, 3) * cornerW;
                    cornersWorld[i].z = canvasMatrix.GetElement(2, 0) * cornerX + canvasMatrix.GetElement(2, 1) * cornerY + canvasMatrix.GetElement(2, 2) * cornerZ + canvasMatrix.GetElement(2, 3) * cornerW;
                    float w = canvasMatrix.GetElement(3, 0) * cornerX + canvasMatrix.GetElement(3, 1) * cornerY + canvasMatrix.GetElement(3, 2) * cornerZ + canvasMatrix.GetElement(3, 3) * cornerW;
                    if (std::abs(w) > 1e-5f) {
                        cornersWorld[i].x /= w;
                        cornersWorld[i].y /= w;
                        cornersWorld[i].z /= w;
                    }
                }
                
                // Project corners to screen space and draw wireframe
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 screenCorners[4];
                bool cornersValid[4] = { false, false, false, false };
                
                for (int i = 0; i < 4; ++i) {
                    cornersValid[i] = WorldToScreen(cornersWorld[i], view, proj, panelPos, panelSize, screenCorners[i]);
                }
                
                // Draw wireframe rectangle (only draw lines between valid corners)
                ImU32 lineColor = IM_COL32(0, 0, 255, 255);  // Dark blue, fully opaque
                float lineThickness = 1.5f;
                
                for (int i = 0; i < 4; ++i) {
                    int next = (i + 1) % 4;
                    if (cornersValid[i] && cornersValid[next]) {
                        drawList->AddLine(screenCorners[i], screenCorners[next], lineColor, lineThickness);
                    }
                }
            }
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

        // Check if stretch anchors are active (Unity behavior: can't resize with gizmo when stretched)
        bool isStretchedX = (std::abs(rectTransform.anchorMinX - rectTransform.anchorMaxX) > 0.001f);
        bool isStretchedY = (std::abs(rectTransform.anchorMinY - rectTransform.anchorMaxY) > 0.001f);
        bool hasStretchAnchors = isStretchedX || isStretchedY;

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
        // worldTransform.x/y is the pivot position in screen pixels (1920x1080 space)
        // These are already in absolute screen coordinates (canvas position is included via anchor calculation)
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

        // Draw corner handles - only when NOT using stretch anchors
        if (!hasStretchAnchors) {
            for (int i = 0; i < 4; i++) {
                drawList->AddRectFilled(
                    ImVec2(corners[i].x - handleSize * 0.5f, corners[i].y - handleSize * 0.5f),
                    ImVec2(corners[i].x + handleSize * 0.5f, corners[i].y + handleSize * 0.5f),
                    IM_COL32(0, 120, 255, 255)
                );
            }
        }

        // Draw edge handles - only when NOT using stretch anchors
        if (!hasStretchAnchors) {
            for (int i = 0; i < 4; i++) {
                drawList->AddRectFilled(
                    ImVec2(edges[i].x - handleSize * 0.5f, edges[i].y - handleSize * 0.5f),
                    ImVec2(edges[i].x + handleSize * 0.5f, edges[i].y + handleSize * 0.5f),
                    IM_COL32(0, 120, 255, 255)
                );
            }
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

            // Only allow corner/edge hovering when NOT using stretch anchors
            if (!hasStretchAnchors) {
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
            }

            // Center/pivot handle - only allow hovering when NOT using stretch both
            // (Stretch horizontal/vertical still allow movement on one axis)
            if (!hoveringHandle && !(isStretchedX && isStretchedY))
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

            // Corner handles - only when NOT using stretch anchors
            if (!handleClicked && !hasStretchAnchors) {
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

            // Edge handles - only when NOT using stretch anchors
            if (!handleClicked && !hasStretchAnchors)
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

            // Center/pivot handle (position drag) - only when NOT using stretch both
            // (Stretch horizontal/vertical still allow movement on one axis)
            if (!handleClicked && !(isStretchedX && isStretchedY))
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
        // Use IsMouseDown instead of IsMouseDragging to work even when mouse is outside viewport
        if (s_isDraggingRotation && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Get current mouse position (works even when outside viewport)
            ImVec2 currentMousePos = ImGui::GetMousePos();
            float currentAngle = GetAngleFromCenter(s_rotationCenter, currentMousePos);
            
            // Calculate delta from last frame's angle to handle -180/180 wrapping
            // Instead of using s_rotationStartAngle directly, we track cumulative rotation
            float frameDelta = currentAngle - s_rotationStartAngle;
            
            // Normalize frame delta to [-180, 180] to handle wrapping
            while (frameDelta > 180.0f) frameDelta -= 360.0f;
            while (frameDelta < -180.0f) frameDelta += 360.0f;
            
            // Apply rotation sensitivity multiplier for world space UI
            // Higher values = more sensitive (less mouse movement needed for same rotation)
            // This function is Update2DGizmoWorldSpace, so we're always in world space here
            const float rotationSensitivity = 3.0f;  // 3x sensitivity for world space
            frameDelta *= rotationSensitivity;
            
            // Accumulate the frame delta to get total rotation since drag started
            s_cumulativeRotation += frameDelta;
            
            // Update start angle for next frame (using current angle to avoid accumulation errors)
            s_rotationStartAngle = currentAngle;
            
            // Apply cumulative rotation to the entity's local rotation
            // For screen space overlay, normalize to [-180, 180] range
            rectTransform.rotationZ = s_originalRotation + s_cumulativeRotation;
            
            // Normalize rotation to [-180, 180] for screen space overlay only
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

            // Unity behavior: Apply constraints based on stretch anchors
            // - Stretch Horizontal: Only allow Y movement (X is constrained by anchors)
            // - Stretch Vertical: Only allow X movement (Y is constrained by anchors)
            // - Stretch Both: Position is locked (element fills parent)
            if (isStretchedX && isStretchedY) {
                // Stretch Both: Position is locked, don't apply any movement
                // Keep original position
                rectTransform.x = s_originalTransform.x;
                rectTransform.y = s_originalTransform.y;
            } else if (isStretchedX) {
                // Stretch Horizontal: Only allow Y movement
                rectTransform.x = s_originalTransform.x;  // Lock X
                rectTransform.y = s_originalTransform.y + deltaFBY;  // Allow Y
            } else if (isStretchedY) {
                // Stretch Vertical: Only allow X movement
                rectTransform.x = s_originalTransform.x + deltaFBX;  // Allow X
                rectTransform.y = s_originalTransform.y;  // Lock Y
            } else {
                // Point anchors: Allow movement in both directions
                rectTransform.x = s_originalTransform.x + deltaFBX;
                rectTransform.y = s_originalTransform.y + deltaFBY;
            }

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

    void UIGizmoHandler::Update2DGizmoWorldSpace(uint32_t uiEntityId,
        const NE::Math::Mat4& view,
        const NE::Math::Mat4& proj,
        ImVec2 panelPos,
        ImVec2 panelSize)
    {
        // Begin gizmo if not already active
        if (!s_gizmoActive) {
            Begin2DGizmo(uiEntityId);
        }
        
        if (!s_gizmoActive || s_gizmoType != 1 || s_gizmoEntityId != uiEntityId) return;

        auto& rectTransform = NE::ECS::Command::GetUIRectTransform(uiEntityId);

        // Check if stretch anchors are active (Unity behavior: can't resize with gizmo when stretched)
        bool isStretchedX = (std::abs(rectTransform.anchorMinX - rectTransform.anchorMaxX) > 0.001f);
        bool isStretchedY = (std::abs(rectTransform.anchorMinY - rectTransform.anchorMaxY) > 0.001f);
        bool hasStretchAnchors = isStretchedX || isStretchedY;

        // Get world transform to find the pivot position in world space
        auto worldTransform = NE::ECS::Query::GetUIWorldTransform(uiEntityId);
        
        // Get the accumulated transform to build the model matrix
        // Find canvas entity
        uint32_t canvasEntityId = std::numeric_limits<uint32_t>::max();
        NE::ECS::Component::UICanvas* canvas = nullptr;
        
        if (NE::ECS::Query::HasUICanvas(uiEntityId)) {
            canvasEntityId = uiEntityId;
            canvas = &NE::ECS::Command::GetUICanvas(uiEntityId);
        } else {
            uint32_t p = rectTransform.parent;
            while (p != std::numeric_limits<uint32_t>::max()) {
                if (NE::ECS::Query::HasUICanvas(p)) {
                    canvasEntityId = p;
                    canvas = &NE::ECS::Command::GetUICanvas(p);
                    break;
                }
                if (!NE::ECS::Query::HasUIRectTransform(p)) break;
                p = NE::ECS::Query::GetUIRectTransform(p).parent;
            }
        }
        
        if (!canvas || canvas->renderMode != NE::ECS::Component::UICanvas::RenderMode::WORLD_SPACE) {
            return;
        }

        // Build the full model matrix to transform unit quad corners to world space
        // Use the same logic as BuildWorldSpaceModelMatrix in UITransformSystem
        const float PI = 3.14159265358979f;
        
        // Get accumulated rotation and scale by building parent chain (needed for rotation matrix)
        NE::Math::Vec3 accumulatedScale(1, 1, 1);
        float accumulatedRotX = 0.0f;
        float accumulatedRotY = 0.0f;
        float accumulatedRotZ = 0.0f;
        
        // Build parent chain
        std::vector<uint32_t> chain;
        uint32_t current = uiEntityId;
        while (current != std::numeric_limits<uint32_t>::max() && NE::ECS::Query::HasUIRectTransform(current)) {
            chain.push_back(current);
            if (current == canvasEntityId) break;
            current = NE::ECS::Query::GetUIRectTransform(current).parent;
        }
        std::reverse(chain.begin(), chain.end());
        
        // Accumulate rotation and scale (position comes from worldTransform which accounts for anchors)
        for (uint32_t entity : chain) {
            auto& currentRect = NE::ECS::Query::GetUIRectTransform(entity);
            accumulatedScale.x *= currentRect.scaleX;
            accumulatedScale.y *= currentRect.scaleY;
            accumulatedScale.z *= currentRect.scaleZ;
            accumulatedRotX += currentRect.rotationX;
            accumulatedRotY += currentRect.rotationY;
            accumulatedRotZ += currentRect.rotationZ;
        }
        
        // Build model matrix using same logic as BuildWorldSpaceModelMatrix
        // Use worldTransform position and size which already accounts for anchors
        float pivotX = rectTransform.pivotX;
        float pivotY = rectTransform.pivotY;
        // worldTransform.width/height are already scaled and account for stretched anchors
        float scaledWidth = worldTransform.width;
        float scaledHeight = worldTransform.height;
        
        // Step 1: Scale
        NE::Math::Mat4 scaleMatrix = NE::Math::Mat4::BuildScaling(scaledWidth, scaledHeight, accumulatedScale.z);
        
        // Step 2: Apply pivot offset (same as BuildWorldSpaceModelMatrix)
        float pivotOffsetX = -scaledWidth * pivotX;
        float pivotOffsetY = -scaledHeight * (1.0f - pivotY);  // Flip Y for world space
        NE::Math::Mat4 pivotMatrix = NE::Math::Mat4::BuildTranslation(pivotOffsetX, pivotOffsetY, 0.0f);
        
        // Step 3: Rotation (full 3D) - order: X * Y * Z (same as TransformSystem)
        NE::Math::Mat4 rotationX = NE::Math::Mat4::BuildXRotation(accumulatedRotX * PI / 180.0f);
        NE::Math::Mat4 rotationY = NE::Math::Mat4::BuildYRotation(accumulatedRotY * PI / 180.0f);
        NE::Math::Mat4 rotationZ = NE::Math::Mat4::BuildZRotation(accumulatedRotZ * PI / 180.0f);
        NE::Math::Mat4 rotationMatrix = rotationX * rotationY * rotationZ;
        
        // Step 4: Translation - use worldTransform position (already accounts for anchors)
        NE::Math::Mat4 translationMatrix = NE::Math::Mat4::BuildTranslation(
            worldTransform.x, worldTransform.y, worldTransform.z
        );
        
        // Full model matrix: Translate * Rotate * PivotOffset * Scale (same as BuildWorldSpaceModelMatrix)
        NE::Math::Mat4 modelMatrix = translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
        
        // Unit quad corners in local space: (0,0,0), (1,0,0), (1,1,0), (0,1,0)
        NE::Math::Vec3 unitCorners[4] = {
            NE::Math::Vec3(0.0f, 0.0f, 0.0f),  // Top-left (Y-down)
            NE::Math::Vec3(1.0f, 0.0f, 0.0f),  // Top-right
            NE::Math::Vec3(1.0f, 1.0f, 0.0f),  // Bottom-right
            NE::Math::Vec3(0.0f, 1.0f, 0.0f)   // Bottom-left
        };
        
        // Calculate center (pivot) in screen space FIRST (needed for invalid corner fallback)
        // Use worldTransform position which already accounts for anchors
        NE::Math::Vec3 pivotWorldPos(worldTransform.x, worldTransform.y, worldTransform.z);
        ImVec2 center;
        bool centerValid = WorldToScreen(pivotWorldPos, view, proj, panelPos, panelSize, center);
        
        // If center not valid, use viewport center as fallback
        if (!centerValid) {
            center = ImVec2(panelPos.x + panelSize.x * 0.5f, panelPos.y + panelSize.y * 0.5f);
        }
        
        // Transform corners to world space using model matrix
        ImVec2 corners[4];
        bool cornersValid[4] = { false, false, false, false };
        int validCornerCount = 0;
        
        for (int i = 0; i < 4; i++) {
            // Manually multiply Mat4 * Vec4 (avoiding Vec4 constructor)
            float inputX = unitCorners[i].x;
            float inputY = unitCorners[i].y;
            float inputZ = unitCorners[i].z;
            float inputW = 1.0f;
            
            float worldX = modelMatrix.GetElement(0, 0) * inputX + modelMatrix.GetElement(0, 1) * inputY + modelMatrix.GetElement(0, 2) * inputZ + modelMatrix.GetElement(0, 3) * inputW;
            float worldY = modelMatrix.GetElement(1, 0) * inputX + modelMatrix.GetElement(1, 1) * inputY + modelMatrix.GetElement(1, 2) * inputZ + modelMatrix.GetElement(1, 3) * inputW;
            float worldZ = modelMatrix.GetElement(2, 0) * inputX + modelMatrix.GetElement(2, 1) * inputY + modelMatrix.GetElement(2, 2) * inputZ + modelMatrix.GetElement(2, 3) * inputW;
            
            NE::Math::Vec3 worldCorner(worldX, worldY, worldZ);
            
            // Project to screen space
            ImVec2 projectedCorner;
            if (WorldToScreen(worldCorner, view, proj, panelPos, panelSize, projectedCorner)) {
                // Check if corner is within viewport (with small margin for edge cases)
                const float margin = 50.0f;  // Increased margin to allow slight out-of-viewport
                bool inViewport = (projectedCorner.x >= panelPos.x - margin && projectedCorner.x <= panelPos.x + panelSize.x + margin &&
                                   projectedCorner.y >= panelPos.y - margin && projectedCorner.y <= panelPos.y + panelSize.y + margin);
                
                if (inViewport) {
                    corners[i] = projectedCorner;
                    cornersValid[i] = true;
                    validCornerCount++;
                } else {
                    // Corner is far outside viewport - mark as invalid and don't store position
                    // This prevents edges from being calculated with invalid positions
                    corners[i] = center;  // Use center as placeholder (won't be drawn)
                    cornersValid[i] = false;
                }
            } else {
                // Behind camera or invalid projection - mark as invalid
                corners[i] = center;  // Use center as placeholder (won't be drawn)
                cornersValid[i] = false;
            }
        }
        
        // Draw gizmo if ANY part of the UI is visible (at least 1 corner or center)
        // Only skip if the ENTIRE image is out of screen (all corners AND center invalid)
        if (validCornerCount == 0 && !centerValid) {
            return;  // Entire UI element is out of view, don't draw gizmo
        }
        
        // If center not valid but we have valid corners, estimate center from valid corners
        if (!centerValid && validCornerCount > 0) {
            float avgX = 0.0f, avgY = 0.0f;
            int count = 0;
            for (int i = 0; i < 4; i++) {
                if (cornersValid[i]) {
                    avgX += corners[i].x;
                    avgY += corners[i].y;
                    count++;
                }
            }
            if (count > 0) {
                center = ImVec2(avgX / count, avgY / count);
                centerValid = true;
            }
        }
        
        // If we have valid corners but center is still invalid, use viewport center as fallback
        if (!centerValid) {
            center = ImVec2(panelPos.x + panelSize.x * 0.5f, panelPos.y + panelSize.y * 0.5f);
        }
        
        // Calculate edge midpoints - only for edges between valid corners
        ImVec2 edges[4];
        bool edgesValid[4] = { false, false, false, false };
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            if (cornersValid[i] && cornersValid[next]) {
                edges[i] = ImVec2((corners[i].x + corners[next].x) * 0.5f, (corners[i].y + corners[next].y) * 0.5f);
                edgesValid[i] = true;
            } else {
                // Invalid edge - set to center as fallback (won't be drawn anyway)
                edges[i] = center;
                edgesValid[i] = false;
            }
        }
        
        const float handleSize = 8.0f;
        ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Check if there's any rotation (X, Y, or Z)
        bool hasRotation = (std::abs(accumulatedRotX) > 0.001f || 
                           std::abs(accumulatedRotY) > 0.001f || 
                           std::abs(accumulatedRotZ) > 0.001f);
        
        // For rotation handle direction, use Z rotation (main rotation for UI)
        float rotZ = accumulatedRotZ * PI / 180.0f;
        float cosR = std::cos(rotZ);
        float sinR = std::sin(rotZ);
        
        // ========== DRAW GIZMO ==========
        
        // Draw rectangle outline - only draw edges when BOTH corners are valid
        for (int i = 0; i < 4; i++) {
            int next = (i + 1) % 4;
            // Only draw edge if BOTH corners are valid (prevents squashed appearance)
            if (cornersValid[i] && cornersValid[next]) {
                drawList->AddLine(corners[i], corners[next], IM_COL32(255, 255, 255, 255), 2.0f);
            }
        }
        
        // Draw corner handles - only for valid corners and when NOT using stretch anchors
        if (!hasStretchAnchors) {
            for (int i = 0; i < 4; i++) {
                if (cornersValid[i]) {
                    drawList->AddRectFilled(
                        ImVec2(corners[i].x - handleSize * 0.5f, corners[i].y - handleSize * 0.5f),
                        ImVec2(corners[i].x + handleSize * 0.5f, corners[i].y + handleSize * 0.5f),
                        IM_COL32(0, 120, 255, 255)
                    );
                }
            }
        }
        
        // Draw edge handles - only for valid edges and when NOT using stretch anchors
        if (!hasStretchAnchors) {
            for (int i = 0; i < 4; i++) {
                if (edgesValid[i]) {
                    drawList->AddRectFilled(
                        ImVec2(edges[i].x - handleSize * 0.5f, edges[i].y - handleSize * 0.5f),
                        ImVec2(edges[i].x + handleSize * 0.5f, edges[i].y + handleSize * 0.5f),
                        IM_COL32(0, 120, 255, 255)
                    );
                }
            }
        }
        
        // Draw center/pivot handle
        drawList->AddCircleFilled(center, handleSize * 0.5f, IM_COL32(255, 120, 0, 255));
        
        // ========== ROTATION HANDLE ==========
        // Find the top edge (edge with minimum Y value in screen space)
        // In ImGui, Y increases downward, so top edge has smallest Y
        int topEdgeIndex = -1;
        float minY = std::numeric_limits<float>::max();
        for (int i = 0; i < 4; i++) {
            if (edgesValid[i] && edges[i].y < minY) {
                minY = edges[i].y;
                topEdgeIndex = i;
            }
        }
        
        bool canDrawRotationHandle = (topEdgeIndex >= 0);
        bool hoveringRotation = false;
        
        if (canDrawRotationHandle) {
            float rotationHandleOffset = 35.0f;
            ImVec2 rotationHandleBase = edges[topEdgeIndex];
            ImVec2 rotationHandle;
            
            if (hasRotation) {
                float dirX = -sinR;
                float dirY = -cosR;
                rotationHandle.x = rotationHandleBase.x + dirX * rotationHandleOffset;
                rotationHandle.y = rotationHandleBase.y + dirY * rotationHandleOffset;
            }
            else {
                rotationHandle.x = center.x;
                rotationHandle.y = edges[topEdgeIndex].y - rotationHandleOffset;  // Above top edge
            }
            
            drawList->AddLine(rotationHandleBase, rotationHandle, IM_COL32(255, 255, 255, 180), 1.5f);
            
            const float rotationHandleRadius = 10.0f;
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
        }
        
        // ========== HOVERING DETECTION ==========
        bool mouseInThisPanel = mousePos.x >= panelPos.x && mousePos.x <= panelPos.x + panelSize.x &&
            mousePos.y >= panelPos.y && mousePos.y <= panelPos.y + panelSize.y;
        
        if (!s_isDraggingUI && s_draggingCorner < 0 && s_draggingEdge < 0 && !s_isDraggingRotation && mouseInThisPanel)
        {
            bool hoveringHandle = false;
            
            if (hoveringRotation) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                hoveringHandle = true;
            }
            
            // Only allow corner/edge hovering when NOT using stretch anchors
            if (!hasStretchAnchors) {
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
            }
            
            // Center/pivot handle - only allow hovering when NOT using stretch both
            // (Stretch horizontal/vertical still allow movement on one axis)
            if (!hoveringHandle && !(isStretchedX && isStretchedY))
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
        
        // ========== HELPER: Screen to World (unproject) ==========
        auto ScreenToWorld = [&](const ImVec2& screenPos, float worldZ) -> NE::Math::Vec3 {
            // Convert screen to NDC
            float u = (screenPos.x - panelPos.x) / panelSize.x;  // [0..1]
            float v = 1.0f - (screenPos.y - panelPos.y) / panelSize.y;  // [0..1], flip Y
            float ndcX = u * 2.0f - 1.0f;  // [-1..1]
            float ndcY = v * 2.0f - 1.0f;  // [-1..1]
            
            // Unproject: inverse of view-projection
            NE::Math::Mat4 VP = proj * view;
            NE::Math::Mat4 invVP = VP.Inverse();
            
            // Create two points on the ray (near and far in NDC Z)
            float nearNDCZ = -1.0f;
            float farNDCZ = 1.0f;
            
            // Unproject near point (as Vec4, then divide by w)
            float nearX4 = invVP.GetElement(0, 0) * ndcX + invVP.GetElement(0, 1) * ndcY + invVP.GetElement(0, 2) * nearNDCZ + invVP.GetElement(0, 3);
            float nearY4 = invVP.GetElement(1, 0) * ndcX + invVP.GetElement(1, 1) * ndcY + invVP.GetElement(1, 2) * nearNDCZ + invVP.GetElement(1, 3);
            float nearZ4 = invVP.GetElement(2, 0) * ndcX + invVP.GetElement(2, 1) * ndcY + invVP.GetElement(2, 2) * nearNDCZ + invVP.GetElement(2, 3);
            float nearW4 = invVP.GetElement(3, 0) * ndcX + invVP.GetElement(3, 1) * ndcY + invVP.GetElement(3, 2) * nearNDCZ + invVP.GetElement(3, 3);
            
            float nearX = 0.0f, nearY = 0.0f, nearZ = 0.0f;
            if (std::abs(nearW4) > 1e-5f) {
                nearX = nearX4 / nearW4;
                nearY = nearY4 / nearW4;
                nearZ = nearZ4 / nearW4;
            }
            
            // Unproject far point
            float farX4 = invVP.GetElement(0, 0) * ndcX + invVP.GetElement(0, 1) * ndcY + invVP.GetElement(0, 2) * farNDCZ + invVP.GetElement(0, 3);
            float farY4 = invVP.GetElement(1, 0) * ndcX + invVP.GetElement(1, 1) * ndcY + invVP.GetElement(1, 2) * farNDCZ + invVP.GetElement(1, 3);
            float farZ4 = invVP.GetElement(2, 0) * ndcX + invVP.GetElement(2, 1) * ndcY + invVP.GetElement(2, 2) * farNDCZ + invVP.GetElement(2, 3);
            float farW4 = invVP.GetElement(3, 0) * ndcX + invVP.GetElement(3, 1) * ndcY + invVP.GetElement(3, 2) * farNDCZ + invVP.GetElement(3, 3);
            
            float farX = 0.0f, farY = 0.0f, farZ = 0.0f;
            if (std::abs(farW4) > 1e-5f) {
                farX = farX4 / farW4;
                farY = farY4 / farW4;
                farZ = farZ4 / farW4;
            }
            
            // Find intersection with plane at worldZ
            // Ray: P = near + t * (far - near)
            // Plane: z = worldZ
            float rayDirZ = farZ - nearZ;
            if (std::abs(rayDirZ) < 1e-5f) {
                // Ray is parallel to plane, return near point projected to plane
                return NE::Math::Vec3(nearX, nearY, worldZ);
            }
            
            float t = (worldZ - nearZ) / rayDirZ;
            float worldX = nearX + t * (farX - nearX);
            float worldY = nearY + t * (farY - nearY);
            
            return NE::Math::Vec3(worldX, worldY, worldZ);
        };
        
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
                s_originalRotation = rectTransform.rotationZ;  // Use local rotation, not accumulated
                s_cumulativeRotation = 0.0f;  // Reset cumulative rotation
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
            
            // Corner handles - only when NOT using stretch anchors
            if (!handleClicked && !hasStretchAnchors) {
                for (int i = 0; i < 4; ++i)
                {
                    float dx = mousePos.x - corners[i].x;
                    float dy = mousePos.y - corners[i].y;
                    float dist2 = dx * dx + dy * dy;
                    
                    if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                    {
                        s_draggingCorner = i;
                        s_dragStart = mousePos;
                        s_originalWorldPos3D = ScreenToWorld(mousePos, pivotWorldPos.z);
                        s_originalPivotZ3D = pivotWorldPos.z;  // Store original Z for delta calculation
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
            
            // Edge handles - only when NOT using stretch anchors
            if (!handleClicked && !hasStretchAnchors)
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
                        s_originalWorldPos3D = ScreenToWorld(mousePos, pivotWorldPos.z);
                        s_originalPivotZ3D = pivotWorldPos.z;  // Store original Z for delta calculation
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
            
            // Center/pivot handle (position drag) - only when NOT using stretch both
            // (Stretch horizontal/vertical still allow movement on one axis)
            if (!handleClicked && !(isStretchedX && isStretchedY))
            {
                float dx = mousePos.x - center.x;
                float dy = mousePos.y - center.y;
                float dist2 = dx * dx + dy * dy;
                
                if (dist2 <= (handleSize * 0.5f) * (handleSize * 0.5f))
                {
                    s_isDraggingUI = true;
                    s_dragStart = mousePos;
                    s_originalWorldPos3D = ScreenToWorld(mousePos, pivotWorldPos.z);
                    s_originalPivotZ3D = pivotWorldPos.z;  // Store original Z for delta calculation
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
        // Use IsMouseDown instead of IsMouseDragging to work even when mouse is outside viewport
        if (s_isDraggingRotation && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Get current mouse position (works even when outside viewport)
            ImVec2 currentMousePos = ImGui::GetMousePos();
            float currentAngle = GetAngleFromCenter(s_rotationCenter, currentMousePos);
            
            // Calculate delta from last frame's angle to handle -180/180 wrapping
            // Instead of using s_rotationStartAngle directly, we track cumulative rotation
            float frameDelta = currentAngle - s_rotationStartAngle;
            
            // Normalize frame delta to [-180, 180] to handle wrapping
            while (frameDelta > 180.0f) frameDelta -= 360.0f;
            while (frameDelta < -180.0f) frameDelta += 360.0f;
            
            // Apply rotation sensitivity multiplier for world space UI
            // This function (Update2DGizmoWorldSpace) is only called for world space UI
            // Higher values = more sensitive (less mouse movement needed for same rotation)
            const float rotationSensitivity = 50.0f;  // 50x sensitivity for world space
            frameDelta *= rotationSensitivity;
            
            // Accumulate the frame delta to get total rotation since drag started
            s_cumulativeRotation += frameDelta;
            
            // Update start angle for next frame (using current angle to avoid accumulation errors)
            s_rotationStartAngle = currentAngle;
            
            // Apply cumulative rotation to the entity's local rotation
            // Don't normalize the stored rotation value - allow it to accumulate beyond [-180, 180]
            // Rotation matrices work correctly with any angle value, and this allows continuous rotation
            rectTransform.rotationZ = s_originalRotation + s_cumulativeRotation;
            
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
        // Use IsMouseDown instead of IsMouseDragging to work even when mouse is outside viewport
        if (s_isDraggingUI && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Get current mouse position (works even when outside viewport)
            ImVec2 currentScreenPos = ImGui::GetMousePos();
            
            // Calculate screen delta from the original drag start position
            // This allows translation to continue even when the gizmo goes out of view
            ImVec2 screenDelta = ImVec2(
                currentScreenPos.x - s_dragStart.x,
                currentScreenPos.y - s_dragStart.y
            );
            
            // Convert screen delta to world space delta using delta-based projection
            // This works even when the current position is out of view
            NE::Math::Vec3 originalWorldPos = s_originalWorldPos3D;
            
            // Calculate world position at original screen position + delta
            // Use the stored original Z to ensure consistent projection
            ImVec2 newScreenPos = ImVec2(
                s_dragStart.x + screenDelta.x,
                s_dragStart.y + screenDelta.y
            );
            NE::Math::Vec3 newWorldPos = ScreenToWorld(newScreenPos, s_originalPivotZ3D);
            
            // Calculate the world delta
            NE::Math::Vec3 worldDelta = newWorldPos - originalWorldPos;
            
            // Apply a sensitivity factor (adjust this value to tune sensitivity)
            // Smaller values = less sensitive, larger values = more sensitive
            const float sensitivityFactor = 0.15f;  // Reduced from 0.5f for less sensitivity
            worldDelta.x *= sensitivityFactor;
            worldDelta.y *= sensitivityFactor;
            worldDelta.z *= sensitivityFactor;
            
            // Convert world delta to local space delta
            // Build parent transform chain (excluding current entity)
            std::vector<uint32_t> parentChain;
            uint32_t p = rectTransform.parent;
            while (p != std::numeric_limits<uint32_t>::max() && NE::ECS::Query::HasUIRectTransform(p)) {
                parentChain.push_back(p);
                if (p == canvasEntityId) break;
                p = NE::ECS::Query::GetUIRectTransform(p).parent;
            }
            std::reverse(parentChain.begin(), parentChain.end());
            
            // Build parent transform matrix (rotation and scale only, no translation)
            NE::Math::Vec3 parentScale(1, 1, 1);
            float parentRotX = 0.0f, parentRotY = 0.0f, parentRotZ = 0.0f;
            for (uint32_t parentId : parentChain) {
                auto& parentRect = NE::ECS::Query::GetUIRectTransform(parentId);
                parentScale.x *= parentRect.scaleX;
                parentScale.y *= parentRect.scaleY;
                parentScale.z *= parentRect.scaleZ;
                parentRotX += parentRect.rotationX;
                parentRotY += parentRect.rotationY;
                parentRotZ += parentRect.rotationZ;
            }
            
            // Build inverse parent rotation matrix to convert world delta to local space
            NE::Math::Mat4 invRotX = NE::Math::Mat4::BuildXRotation(-parentRotX * PI / 180.0f);
            NE::Math::Mat4 invRotY = NE::Math::Mat4::BuildYRotation(-parentRotY * PI / 180.0f);
            NE::Math::Mat4 invRotZ = NE::Math::Mat4::BuildZRotation(-parentRotZ * PI / 180.0f);
            NE::Math::Mat4 invParentRot = invRotX * invRotY * invRotZ;  // Inverse rotation order
            
            // Transform world delta to local space (remove parent rotation and scale)
            float localDeltaX = invParentRot.GetElement(0, 0) * worldDelta.x + invParentRot.GetElement(0, 1) * worldDelta.y + invParentRot.GetElement(0, 2) * worldDelta.z;
            float localDeltaY = invParentRot.GetElement(1, 0) * worldDelta.x + invParentRot.GetElement(1, 1) * worldDelta.y + invParentRot.GetElement(1, 2) * worldDelta.z;
            float localDeltaZ = invParentRot.GetElement(2, 0) * worldDelta.x + invParentRot.GetElement(2, 1) * worldDelta.y + invParentRot.GetElement(2, 2) * worldDelta.z;
            
            // Remove parent scale
            if (std::abs(parentScale.x) > 1e-5f) localDeltaX /= parentScale.x;
            if (std::abs(parentScale.y) > 1e-5f) localDeltaY /= parentScale.y;
            if (std::abs(parentScale.z) > 1e-5f) localDeltaZ /= parentScale.z;
            
            // Unity behavior: Apply constraints based on stretch anchors
            // - Stretch Horizontal: Only allow Y movement (X is constrained by anchors)
            // - Stretch Vertical: Only allow X movement (Y is constrained by anchors)
            // - Stretch Both: Position is locked (element fills parent)
            if (isStretchedX && isStretchedY) {
                // Stretch Both: Position is locked, don't apply any movement
                rectTransform.x = s_originalTransform.x;
                rectTransform.y = s_originalTransform.y;
                rectTransform.z = s_originalTransform.z;  // Z can still move (depth)
            } else if (isStretchedX) {
                // Stretch Horizontal: Only allow Y movement
                rectTransform.x = s_originalTransform.x;  // Lock X
                rectTransform.y = s_originalTransform.y + localDeltaY;  // Allow Y
                rectTransform.z = s_originalTransform.z + localDeltaZ;  // Allow Z
            } else if (isStretchedY) {
                // Stretch Vertical: Only allow X movement
                rectTransform.x = s_originalTransform.x + localDeltaX;  // Allow X
                rectTransform.y = s_originalTransform.y;  // Lock Y
                rectTransform.z = s_originalTransform.z + localDeltaZ;  // Allow Z
            } else {
                // Point anchors: Allow movement in all directions
                rectTransform.x = s_originalTransform.x + localDeltaX;
                rectTransform.y = s_originalTransform.y + localDeltaY;
                rectTransform.z = s_originalTransform.z + localDeltaZ;
            }
            
            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }
        
        if (s_isDraggingUI && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_isDraggingUI = false;
            CommitCommand();
        }
        
        // ========== CORNER/EDGE RESIZE DRAG ==========
        // Use IsMouseDown instead of IsMouseDragging to work even when mouse is outside viewport
        if ((s_draggingCorner >= 0 || s_draggingEdge >= 0) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Get current mouse position (works even when outside viewport)
            ImVec2 currentScreenPos = ImGui::GetMousePos();
            ImVec2 screenDelta = ImVec2(
                currentScreenPos.x - s_dragStart.x,
                currentScreenPos.y - s_dragStart.y
            );
            
            // Convert screen delta to world space delta using delta-based projection
            // This works even when the current position is out of view
            ImVec2 newScreenPos = ImVec2(
                s_dragStart.x + screenDelta.x,
                s_dragStart.y + screenDelta.y
            );
            NE::Math::Vec3 newWorldPos = ScreenToWorld(newScreenPos, s_originalPivotZ3D);
            NE::Math::Vec3 worldDelta = newWorldPos - s_originalWorldPos3D;
            
            // Apply sensitivity factor for resize operations
            // Increased from 0.15f to 0.3f for more sensitive scaling
            const float resizeSensitivityFactor = 0.3f;
            worldDelta.x *= resizeSensitivityFactor;
            worldDelta.y *= resizeSensitivityFactor;
            worldDelta.z *= resizeSensitivityFactor;
            
            // Convert world delta to local space (same as position drag)
            std::vector<uint32_t> parentChain;
            uint32_t p = rectTransform.parent;
            while (p != std::numeric_limits<uint32_t>::max() && NE::ECS::Query::HasUIRectTransform(p)) {
                parentChain.push_back(p);
                if (p == canvasEntityId) break;
                p = NE::ECS::Query::GetUIRectTransform(p).parent;
            }
            std::reverse(parentChain.begin(), parentChain.end());
            
            NE::Math::Vec3 parentScale(1, 1, 1);
            float parentRotX = 0.0f, parentRotY = 0.0f, parentRotZ = 0.0f;
            for (uint32_t parentId : parentChain) {
                auto& parentRect = NE::ECS::Query::GetUIRectTransform(parentId);
                parentScale.x *= parentRect.scaleX;
                parentScale.y *= parentRect.scaleY;
                parentScale.z *= parentRect.scaleZ;
                parentRotX += parentRect.rotationX;
                parentRotY += parentRect.rotationY;
                parentRotZ += parentRect.rotationZ;
            }
            
            NE::Math::Mat4 invRotX = NE::Math::Mat4::BuildXRotation(-parentRotX * PI / 180.0f);
            NE::Math::Mat4 invRotY = NE::Math::Mat4::BuildYRotation(-parentRotY * PI / 180.0f);
            NE::Math::Mat4 invRotZ = NE::Math::Mat4::BuildZRotation(-parentRotZ * PI / 180.0f);
            NE::Math::Mat4 invParentRot = invRotX * invRotY * invRotZ;
            
            float localDeltaX = invParentRot.GetElement(0, 0) * worldDelta.x + invParentRot.GetElement(0, 1) * worldDelta.y + invParentRot.GetElement(0, 2) * worldDelta.z;
            float localDeltaY = invParentRot.GetElement(1, 0) * worldDelta.x + invParentRot.GetElement(1, 1) * worldDelta.y + invParentRot.GetElement(1, 2) * worldDelta.z;
            
            // Remove parent scale
            if (std::abs(parentScale.x) > 1e-5f) localDeltaX /= parentScale.x;
            if (std::abs(parentScale.y) > 1e-5f) localDeltaY /= parentScale.y;
            
            // Calculate resize in local space
            float deltaX = localDeltaX;
            float deltaY = localDeltaY;
            
            // For corners: resize both width and height
            // For edges: resize only one dimension
            if (s_draggingCorner >= 0) {
                // Corner resize: adjust width and height based on corner
                // Corner 0: top-left, 1: top-right, 2: bottom-right, 3: bottom-left
                float widthDelta = 0.0f;
                float heightDelta = 0.0f;
                
                if (s_draggingCorner == 0 || s_draggingCorner == 3) {
                    // Left corners: negative X delta increases width
                    widthDelta = -deltaX;
                } else {
                    // Right corners: positive X delta increases width
                    widthDelta = deltaX;
                }
                
                if (s_draggingCorner == 0 || s_draggingCorner == 1) {
                    // Top corners: negative Y delta increases height (Y-up)
                    heightDelta = -deltaY;
                } else {
                    // Bottom corners: positive Y delta increases height
                    heightDelta = deltaY;
                }
                
                rectTransform.width = std::max(1.0f, s_originalTransform.width + widthDelta);
                rectTransform.height = std::max(1.0f, s_originalTransform.height + heightDelta);
            } else if (s_draggingEdge >= 0) {
                // Edge resize: adjust one dimension
                // Edge 0: top, 1: right, 2: bottom, 3: left
                if (s_draggingEdge == 0 || s_draggingEdge == 2) {
                    // Top/bottom edges: adjust height
                    float heightDelta = (s_draggingEdge == 0) ? -deltaY : deltaY;
                    rectTransform.height = std::max(1.0f, s_originalTransform.height + heightDelta);
                } else {
                    // Left/right edges: adjust width
                    float widthDelta = (s_draggingEdge == 3) ? -deltaX : deltaX;
                    rectTransform.width = std::max(1.0f, s_originalTransform.width + widthDelta);
                }
            }
            
            if (s_uiGizmoCmd) {
                s_uiGizmoCmd->SetAfter(rectTransform);
            }
        }
        
        if ((s_draggingCorner >= 0 || s_draggingEdge >= 0) && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            s_draggingCorner = -1;
            s_draggingEdge = -1;
            CommitCommand();
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