#include "pch.h"
#include "UILayoutEngine.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "UITransformUtilities.hpp"
#include <cmath>
#include <limits>

using namespace NE::ECS::Component;

namespace NE::ECS {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float PI = 3.14159265358979f;
    static constexpr float ROTATION_EPSILON = 0.001f;
    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;

    //=========================================================================
    // Constructor
    //=========================================================================

    UILayoutEngine::UILayoutEngine(ComponentManager* cm) : m_cm(cm) {}

    //=========================================================================
    // Transform Hierarchy Functions
    //=========================================================================

    std::vector<Entity> UILayoutEngine::BuildParentChain(
        Entity entity, Entity canvasEntity, UICanvas::RenderMode renderMode
    )
    {
        std::vector<Entity> chain;
        Entity current = entity;

        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            if (current == canvasEntity && renderMode != UICanvas::RenderMode::WORLD_SPACE) {
                break;
            }
            chain.push_back(current);
            if (!m_cm->HasComponent<Hierarchy>(current)) break;
            current = m_cm->GetComponent<Hierarchy>(current).parent;
        }

        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    UILayoutEngine::AccumulatedTransform UILayoutEngine::AccumulateParentTransforms(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        AccumulatedTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        //=====================================================================
        // WORLD SPACE: Position scaled by accumulated parent scale
        //=====================================================================
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

            for (size_t i = 0; i < chain.size(); ++i) {
                Entity current = chain[i];
                auto& rect = m_cm->GetComponent<UIRectTransform>(current);

                // Position is in parent's local space — scale by accumulated parent scale
                result.posX += rect.x * result.scaleX;
                result.posY += rect.y * result.scaleY;
                result.posZ += rect.z * result.scaleZ;

                // Then apply this entity's own scale
                result.scaleX *= rect.scaleX;
                result.scaleY *= rect.scaleY;
                result.scaleZ *= rect.scaleZ;

                // Clamp scale to prevent singular matrices in children
                // A scale of exactly 0 makes child transforms undefined and causes NaN
                static constexpr float MIN_SCALE = 1e-6f;
                if (std::abs(result.scaleX) < MIN_SCALE) result.scaleX = result.scaleX < 0 ? -MIN_SCALE : MIN_SCALE;
                if (std::abs(result.scaleY) < MIN_SCALE) result.scaleY = result.scaleY < 0 ? -MIN_SCALE : MIN_SCALE;
                if (std::abs(result.scaleZ) < MIN_SCALE) result.scaleZ = result.scaleZ < 0 ? -MIN_SCALE : MIN_SCALE;

                // Rotation: sum Euler angles (sufficient for UI — most elements only use Z)
                result.rotationX += rect.rotationX;
                result.rotationY += rect.rotationY;
                result.rotationZ += rect.rotationZ;
            }

            return result;
        }

        //=====================================================================
        // SCREEN SPACE: Apply center anchor and scaleFactor
        //=====================================================================
        result.scaleX = canvas.scaleFactor;
        result.scaleY = canvas.scaleFactor;

        std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity current = chain[i];
            auto& rect = m_cm->GetComponent<UIRectTransform>(current);
            bool isTarget = (current == entity);

            result.scaleX *= rect.scaleX;
            result.scaleY *= rect.scaleY;
            result.scaleZ *= rect.scaleZ;
            result.rotationZ += rect.rotationZ;

            float parentWidth = 0.f;
            float parentHeight = 0.f;

            if (i > 0) {
                Entity parentEntity = chain[i - 1];
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);
                parentWidth = parentRect.width;
                parentHeight = parentRect.height;
            }
            else {
                parentWidth = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth()) / canvas.scaleFactor;
                parentHeight = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight()) / canvas.scaleFactor;
            }

            // Compute anchor-based position
            float localX, localY;

            if (rect.IsStretchedX()) {
                // Stretched: anchors define a region, offsets inset from edges
                float anchoredLeft  = parentWidth * rect.anchorMinX + rect.offsetMinX;
                float anchoredRight = parentWidth * rect.anchorMaxX - rect.offsetMaxX;
                float anchoredWidth = std::max(0.f, anchoredRight - anchoredLeft);
                rect.width = anchoredWidth;
                // Position = left edge of anchored region (pivot applied later for target)
                localX = anchoredLeft;
            } else {
                // Point anchor
                float anchorX = parentWidth * rect.anchorMinX;
                localX = anchorX + rect.x;
            }

            if (rect.IsStretchedY()) {
                float anchoredTop    = parentHeight * rect.anchorMinY + rect.offsetMinY;
                float anchoredBottom = parentHeight * rect.anchorMaxY - rect.offsetMaxY;
                float anchoredHeight = std::max(0.f, anchoredBottom - anchoredTop);
                rect.height = anchoredHeight;
                localY = anchoredTop;
            } else {
                float anchorY = parentHeight * rect.anchorMinY;
                localY = anchorY + rect.y;
            }

            if (isTarget) {
                float scaledWidth = rect.width * result.scaleX;
                float scaledHeight = rect.height * result.scaleY;

                // For point anchors: localX is anchor + offset, subtract pivot
                // For stretched: localX is left edge, add (1-pivot)*width to get pivot position, subtract pivot*scaledWidth
                float finalX, finalY;
                if (rect.IsStretchedX()) {
                    // Stretched: position at left edge + (1-pivot)*width centers correctly
                    finalX = localX;
                } else {
                    finalX = localX - scaledWidth * rect.pivotX;
                }
                if (rect.IsStretchedY()) {
                    finalY = localY;
                } else {
                    finalY = localY - scaledHeight * rect.pivotY;
                }

                float parentRotation = result.rotationZ - rect.rotationZ;
                if (std::abs(parentRotation) > ROTATION_EPSILON) {
                    float rad = parentRotation * PI / 180.0f;
                    float cosR = std::cos(rad);
                    float sinR = std::sin(rad);
                    float rotatedX = finalX * cosR - finalY * sinR;
                    float rotatedY = finalX * sinR + finalY * cosR;
                    finalX = rotatedX;
                    finalY = rotatedY;
                }

                float parentScaleX = result.scaleX / rect.scaleX;
                float parentScaleY = result.scaleY / rect.scaleY;

                result.posX += finalX * parentScaleX;
                result.posY += finalY * parentScaleY;
                result.posZ += rect.z;
            }
            else {
                // Non-target: contribute this entity's TOP-LEFT corner, not its pivot point.
                // result.scaleX has already been multiplied by rect.scaleX, so parent scale = result.scaleX / rect.scaleX.
                float parentScaleX = result.scaleX / rect.scaleX;
                float parentScaleY = result.scaleY / rect.scaleY;
                float posX = rect.IsStretchedX() ? localX : (localX - rect.width  * rect.pivotX * parentScaleX);
                float posY = rect.IsStretchedY() ? localY : (localY - rect.height * rect.pivotY * parentScaleY);
                result.posX += posX;
                result.posY += posY;
                result.posZ += rect.z;
            }
        }

        return result;
    }

    //=========================================================================
    // World Transform Calculation
    //=========================================================================

    UILayoutEngine::WorldTransform UILayoutEngine::CalculateWorldTransform(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    )
    {
        (void)viewMatrix;
        (void)projMatrix;

        WorldTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);

        result.x = accumulated.posX;
        result.y = accumulated.posY;
        result.z = accumulated.posZ;
        result.width = rect.width * accumulated.scaleX;
        result.height = rect.height * accumulated.scaleY;
        result.accumulatedRotationZ = accumulated.rotationZ;
        result.accumulatedScaleX = accumulated.scaleX;
        result.accumulatedScaleY = accumulated.scaleY;

        if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
            canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
            if (canvas.pixelPerfect) {
                ApplyPixelPerfectSnapping(result);
            }
        }

        return result;
    }

    UILayoutEngine::WorldTransform UILayoutEngine::CalculateWorldTransformFromAccumulated(
        Entity entity,
        const UICanvas& canvas,
        const AccumulatedTransform& accumulated
    )
    {
        WorldTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        result.x = accumulated.posX;
        result.y = accumulated.posY;
        result.z = accumulated.posZ;
        result.width = rect.width * accumulated.scaleX;
        result.height = rect.height * accumulated.scaleY;
        result.accumulatedRotationZ = accumulated.rotationZ;
        result.accumulatedScaleX = accumulated.scaleX;
        result.accumulatedScaleY = accumulated.scaleY;

        if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
            canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
            if (canvas.pixelPerfect) {
                ApplyPixelPerfectSnapping(result);
            }
        }

        return result;
    }

    void UILayoutEngine::ApplyPixelPerfectSnapping(WorldTransform& transform)
    {
        transform.x = std::round(transform.x);
        transform.y = std::round(transform.y);
        transform.width = std::round(transform.width);
        transform.height = std::round(transform.height);
    }

    //=========================================================================
    // World Matrix
    //=========================================================================

    void UILayoutEngine::UpdateWorldMatrix(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        if (!rect.worldMatrixDirty) return;

        AccumulatedTransform acc = AccumulateParentTransforms(entity, canvasEntity, canvas);

        Math::Mat4 T = Math::Mat4::BuildTranslation(Math::Vec3(acc.posX, acc.posY, acc.posZ));
        Math::Mat4 R = rect.GetRotationMatrix();
        Math::Mat4 S = Math::Mat4::BuildScaling(acc.scaleX, acc.scaleY, acc.scaleZ);

        float pivotOffsetX = rect.width * rect.pivotX;
        float pivotOffsetY = rect.height * rect.pivotY;
        Math::Mat4 pivotTrans = Math::Mat4::BuildTranslation(Math::Vec3(pivotOffsetX, pivotOffsetY, 0.0f));
        Math::Mat4 pivotTransInv = Math::Mat4::BuildTranslation(Math::Vec3(-pivotOffsetX, -pivotOffsetY, 0.0f));

        rect.worldMatrix = T * pivotTrans * R * S * pivotTransInv;
        rect.worldMatrixDirty = false;
    }

    void UILayoutEngine::UpdateWorldMatrixFromAccumulated(
        Entity entity,
        const AccumulatedTransform& acc,
        bool isWorldSpace
    )
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        if (!rect.worldMatrixDirty) return;

        Math::Mat4 T = Math::Mat4::BuildTranslation(Math::Vec3(acc.posX, acc.posY, acc.posZ));

        // For WorldSpace, build rotation from accumulated angles (full 3D rotation)
        Math::Mat4 R;
        if (isWorldSpace) {
            Math::Mat4 rotX = Math::Mat4::BuildXRotation(acc.rotationX);
            Math::Mat4 rotY = Math::Mat4::BuildYRotation(acc.rotationY);
            Math::Mat4 rotZ = Math::Mat4::BuildZRotation(acc.rotationZ);
            R = rotZ * rotY * rotX;
        } else {
            R = rect.GetRotationMatrix();
        }

        if (isWorldSpace) {
            // WorldSpace: unit quad [0..1], scale includes element dimensions.
            // Pivot is in unit space (0..1), applied BEFORE scale so rotation
            // happens around the correct point on the unit quad.
            Math::Mat4 S = Math::Mat4::BuildScaling(acc.scaleX * rect.width, acc.scaleY * rect.height, acc.scaleZ);

            float unitPivotX = rect.pivotX;   // 0..1
            float unitPivotY = rect.pivotY;   // 0..1
            Math::Mat4 pivotTrans    = Math::Mat4::BuildTranslation(Math::Vec3( unitPivotX,  unitPivotY, 0.0f));
            Math::Mat4 pivotTransInv = Math::Mat4::BuildTranslation(Math::Vec3(-unitPivotX, -unitPivotY, 0.0f));

            // Order: offset pivot in unit space → rotate → restore pivot → scale to world → translate
            rect.worldMatrix = T * S * pivotTrans * R * pivotTransInv;
        } else {
            // Screen-space: vertices pre-positioned in pixels, scale is parent accumulation only.
            // Pivot is in pixel space (width * pivotX, height * pivotY).
            Math::Mat4 S = Math::Mat4::BuildScaling(acc.scaleX, acc.scaleY, acc.scaleZ);

            float pivotOffsetX = rect.width * rect.pivotX;
            float pivotOffsetY = rect.height * rect.pivotY;
            Math::Mat4 pivotTrans    = Math::Mat4::BuildTranslation(Math::Vec3( pivotOffsetX,  pivotOffsetY, 0.0f));
            Math::Mat4 pivotTransInv = Math::Mat4::BuildTranslation(Math::Vec3(-pivotOffsetX, -pivotOffsetY, 0.0f));

            rect.worldMatrix = T * pivotTrans * R * S * pivotTransInv;
        }
        rect.worldMatrixDirty = false;
    }

    //=========================================================================
    // Canvas & Scaling
    //=========================================================================

    float UILayoutEngine::CalculateScaleFactor(const UICanvas& canvas)
    {
        return UIUtil::CalculateScaleFactor(canvas);
    }

    //=========================================================================
    // Utility Helpers
    //=========================================================================

    Entity UILayoutEngine::FindOwningCanvas(Entity entity) const
    {
        Entity current = m_cm->HasComponent<Hierarchy>(entity)
            ? m_cm->GetComponent<Hierarchy>(entity).parent
            : NO_ENTITY;

        while (current != NO_ENTITY) {
            if (m_cm->HasComponent<UICanvas>(current)) {
                return current;
            }
            if (!m_cm->HasComponent<Hierarchy>(current)) break;
            current = m_cm->GetComponent<Hierarchy>(current).parent;
        }

        return NO_ENTITY;
    }

    void UILayoutEngine::ComputeAndCacheWorldRect(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) return;

        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);
        WorldTransform wt = CalculateWorldTransformFromAccumulated(entity, canvas, accumulated);

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        rect.cachedWorldX = wt.x;
        rect.cachedWorldY = wt.y;
        rect.cachedWorldWidth = wt.width;
        rect.cachedWorldHeight = wt.height;
        rect.cachedWorldRotZ = wt.accumulatedRotationZ;
        rect.cachedWorldScaleX = wt.accumulatedScaleX;
        rect.cachedWorldScaleY = wt.accumulatedScaleY;
        rect.worldRectCached = true;
    }

    UILayoutEngine::WorldRect UILayoutEngine::GetWorldRect(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    )
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return {};
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        if (rect.worldRectCached) {
            return { rect.cachedWorldX, rect.cachedWorldY,
                     rect.cachedWorldWidth, rect.cachedWorldHeight };
        }

        ComputeAndCacheWorldRect(entity, canvasEntity, canvas);
        return { rect.cachedWorldX, rect.cachedWorldY,
                 rect.cachedWorldWidth, rect.cachedWorldHeight };
    }

    UILayoutEngine::WorldRect UILayoutEngine::CalculateContentBounds(Entity entity)
    {
        if (!m_cm->HasComponent<Hierarchy>(entity)) {
            return {};
        }

        auto& hierarchy = m_cm->GetComponent<Hierarchy>(entity);
        if (hierarchy.children.empty()) {
            return {};
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        Entity canvasEntity = FindOwningCanvas(entity);
        if (canvasEntity == NO_ENTITY) return {};
        auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

        for (uint32_t child : hierarchy.children) {
            if (!m_cm->HasComponent<UIRectTransform>(child)) continue;

            WorldRect wr = GetWorldRect(child, canvasEntity, canvas);

            if (wr.x < minX) minX = wr.x;
            if (wr.y < minY) minY = wr.y;
            if (wr.x + wr.width > maxX) maxX = wr.x + wr.width;
            if (wr.y + wr.height > maxY) maxY = wr.y + wr.height;
        }

        if (minX > maxX || minY > maxY) return {};

        return { minX, minY, maxX - minX, maxY - minY };
    }

} // namespace NE::ECS
