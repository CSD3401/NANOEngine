#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../EditorInterface/ECSExports.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace NE::ECS::Systems {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float PI = 3.14159265358979f;
    static constexpr float ROTATION_EPSILON = 0.001f;

    // Default anchor at center for Screen Space modes
    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;

    // Default world space canvas scale
    static constexpr float DEFAULT_WORLD_SPACE_SCALE = 0.1f;
    
    // Default world space canvas size in world units
    static constexpr float DEFAULT_WORLD_SPACE_CANVAS_WIDTH = 10.0f;
    static constexpr float DEFAULT_WORLD_SPACE_CANVAS_HEIGHT = 10.0f;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    UITransformSystem::UITransformSystem(ComponentManager* cm) : m_cm(cm) {}

    void UITransformSystem::OnEntityAdded(Entity e) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(e)) return;

        auto& rect = m_cm->GetComponent<Component::UIRectTransform>(e);

        // register luid for entity mapping
        if (rect.luid != 0)
        {
            m_luidToEntity[rect.luid] = e;
        }

        // queue parent resolution if needed
        if (rect.parentLuid != 0) 
        {
            m_pendingParents.push_back(PendingParent{ e, rect.parentLuid });
        }
    }

    void UITransformSystem::OnEntityRemoved(Entity e) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(e)) return;

        auto& rect = m_cm->GetComponent<Component::UIRectTransform>(e);

        // remove from luid map
        if (rect.luid != 0)
        {
            m_luidToEntity.erase(rect.luid);
        }

        // remove from pending parents list
        m_pendingParents.erase(
            std::remove_if(m_pendingParents.begin(), m_pendingParents.end(),
                [e](const PendingParent& pp) { return pp.child == e; }),
            m_pendingParents.end()
        );

        // REMOVED THE CANVAS DESTRUCTION LOGIC - Let the editor handle hierarchy
        // The editor's DeleteEntityCommand already handles destroying descendants

        // Just orphan direct children (they may not be destroyed)
        const auto& entities = GetEntities();
        for (Entity child : entities)
        {
            if (child == e) continue;
            if (!m_cm->HasComponent<Component::UIRectTransform>(child)) continue;
            auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(child);
            if (childRect.parent == e)
            {
                childRect.parent = NO_ENTITY;
                childRect.parentLuid = 0;
            }
        }
    }

    void UITransformSystem::Init() {
        ResolvePendingParents();
    }

    void UITransformSystem::ResolvePendingParents() {
        std::vector<PendingParent> stillPending;
        stillPending.reserve(m_pendingParents.size());

        for (const PendingParent& pp : m_pendingParents) {
            if (!m_cm->HasComponent<Component::UIRectTransform>(pp.child))
                continue;

            auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(pp.child);

            // look up parent entity by luid
            auto it = m_luidToEntity.find(pp.parentLuid);
            if (it != m_luidToEntity.end()) 
            {
                Entity parentEnt = it->second;
                childRect.parent = parentEnt;
            }
            else 
            {
                stillPending.push_back(pp);
            }
        }

        m_pendingParents.swap(stillPending);
    }

    void UITransformSystem::SetParent(Entity child, Entity newParent) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(child)) return;

        auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(child);
        childRect.parent = newParent;

        if (newParent != NO_ENTITY && m_cm->HasComponent<Component::UIRectTransform>(newParent)) 
        {
            auto& parentRect = m_cm->GetComponent<Component::UIRectTransform>(newParent);
            childRect.parentLuid = parentRect.luid;
        }
        else 
        {
            childRect.parentLuid = 0;
        }
    }

    Entity UITransformSystem::GetEntityFromLUID(uint64_t luid) const {
        auto it = m_luidToEntity.find(luid);
        if (it != m_luidToEntity.end()) 
        {
            return it->second;
        }
        return NO_ENTITY;
    }

    void UITransformSystem::Update(double) {
        // Future: calculate world-space transforms for UI elements
        // Similar to how TransformSystem builds world matrices
        // shift matrix calculations from uigizmo handling here 
        // and also matrix calculations done in scene panel
        // TODO
    }

    void UITransformSystem::Exit() {}

    //=========================================================================
    // Transform Hierarchy Functions
    //=========================================================================

    bool UITransformSystem::ShouldIncludeCanvasTransform(Component::UICanvas::RenderMode renderMode) {
        return renderMode == Component::UICanvas::RenderMode::WORLD_SPACE;
    }

    std::vector<Entity> UITransformSystem::BuildParentChain(
        Entity entity,
        Entity canvasEntity,
        Component::UICanvas::RenderMode renderMode
    ) {
        std::vector<Entity> chain;
        Entity current = entity;

        while (current != NO_ENTITY && m_cm->HasComponent<Component::UIRectTransform>(current)) {
            if (current == canvasEntity && !ShouldIncludeCanvasTransform(renderMode)) {
                break;
            }

            chain.push_back(current);
            current = m_cm->GetComponent<Component::UIRectTransform>(current).parent;
        }

        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    UITransformSystem::AccumulatedTransform UITransformSystem::AccumulateParentTransforms(
        Entity entity,
        Entity canvasEntity,
        const Component::UICanvas& canvas
    ) {
        AccumulatedTransform result;

        if (!m_cm->HasComponent<Component::UIRectTransform>(entity)) {
            return result;
        }

        //=====================================================================
        // WORLD SPACE: Anchoring system similar to screen space
        // Canvas transform IS included (position, rotation, scale all apply)
        // Anchors work relative to parent's RectTransform bounds (in world units)
        //=====================================================================
        if (canvas.renderMode == Component::UICanvas::RenderMode::WORLD_SPACE) {
            std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

            // Special case: If querying the Canvas itself (root entity, no parent)
            if (entity == canvasEntity) {
                auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);
                // Canvas position (rect.x, rect.y) is already the pivot position in world space
                result.posX = rect.x;
                result.posY = rect.y;
                result.posZ = rect.z;
                result.scaleX = rect.scaleX;
                result.scaleY = rect.scaleY;
                result.scaleZ = rect.scaleZ;
                result.rotationX = rect.rotationX;
                result.rotationY = rect.rotationY;
                result.rotationZ = rect.rotationZ;
                return result;
            }

            for (size_t i = 0; i < chain.size(); ++i) {
                Entity current = chain[i];
                auto& rect = m_cm->GetComponent<Component::UIRectTransform>(current);
                bool isTarget = (current == entity);
                bool isCanvas = (current == canvasEntity);

                // Accumulate scale
                result.scaleX *= rect.scaleX;
                result.scaleY *= rect.scaleY;
                result.scaleZ *= rect.scaleZ;

                // Accumulate rotation
                result.rotationX += rect.rotationX;
                result.rotationY += rect.rotationY;
                result.rotationZ += rect.rotationZ;

                // For canvas entity, just accumulate position directly (no anchor calculation)
                if (isCanvas && !isTarget) {
                    result.posX += rect.x;
                    result.posY += rect.y;
                    result.posZ += rect.z;
                    continue; // Skip anchor calculation for canvas
                }

                // Get parent dimensions and pivot for anchor calculation
                float parentWidth = 0.f;
                float parentHeight = 0.f;
                float parentPivotX = 0.5f;
                float parentPivotY = 0.5f;

                if (i > 0) {
                    // Parent is another UI element - use its width/height and pivot
                    Entity parentEntity = chain[i - 1];
                    auto& parentRect = m_cm->GetComponent<Component::UIRectTransform>(parentEntity);
                    parentWidth = parentRect.width;
                    parentHeight = parentRect.height;
                    parentPivotX = parentRect.pivotX;
                    parentPivotY = parentRect.pivotY;
                }
                else {
                    // Direct child of canvas - use canvas width/height and pivot (in world units)
                    auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);
                    parentWidth = canvasRect.width;
                    parentHeight = canvasRect.height;
                    parentPivotX = canvasRect.pivotX;
                    parentPivotY = canvasRect.pivotY;
                }

                // Calculate anchor position relative to parent pivot
                // In world space: anchor 0.0 = left/bottom edge, 0.5 = center (pivot), 1.0 = right/top edge
                // Anchor position relative to parent pivot: (anchorValue - parentPivot) * parentSize
                bool isStretchedX = rect.IsStretchedX();
                bool isStretchedY = rect.IsStretchedY();

                if (isTarget) {
                    // For the target entity, calculate position and size based on anchor mode
                    if (isStretchedX) {
                        // Stretched horizontally: edges are anchored, size calculated from anchor spread
                        // Canvas width is in world units, but calculatedWidth needs to be in UI pixels
                        // Convert canvas width to UI pixels: divide by canvas scale
                        // Get canvas scale directly (always from canvas, not from accumulated scale)
                        auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);
                        float canvasScaleX = canvasRect.scaleX;
                        
                        // Calculate in world units first (relative to canvas pivot)
                        float anchorMinXRelativeToPivot = (rect.anchorMinX - parentPivotX) * parentWidth;
                        float anchorMaxXRelativeToPivot = (rect.anchorMaxX - parentPivotX) * parentWidth;
                        float leftEdge = anchorMinXRelativeToPivot + rect.offsetMinX;
                        float rightEdge = anchorMaxXRelativeToPivot - rect.offsetMaxX;
                        float calculatedWidthWorld = rightEdge - leftEdge;
                        
                        // Ensure width is positive
                        if (calculatedWidthWorld < 0.0f) {
                            std::swap(leftEdge, rightEdge);
                            calculatedWidthWorld = -calculatedWidthWorld;
                        }
                        
                        // Convert from world units to UI pixels (divide by canvas scale)
                        // This ensures calculatedWidth is in the same units as rect.width (UI pixels)
                        float calculatedWidth = calculatedWidthWorld / canvasScaleX;
                        
                        // Position is the pivot position (in world units)
                        float pivotX = leftEdge + calculatedWidthWorld * rect.pivotX;
                        result.posX += pivotX;
                        result.calculatedWidth = calculatedWidth;
                    }
                    else {
                        // Point anchor: position is anchor relative to parent pivot + offset
                        float anchorXRelativeToPivot = (rect.anchorMinX - parentPivotX) * parentWidth;
                        result.posX += anchorXRelativeToPivot + rect.x;
                    }

                    if (isStretchedY) {
                        // Stretched vertically: edges are anchored, size calculated from anchor spread
                        // In world space (Y-up): anchorMinY=0 is bottom, anchorMaxY=1 is top
                        // Canvas height is in world units, but calculatedHeight needs to be in UI pixels
                        // Convert canvas height to UI pixels: divide by canvas scale
                        // Get canvas scale directly (before child's scale is applied)
                        float canvasScaleY = 1.0f;
                        if (i == 0) {
                            // Direct child of canvas - get canvas scale directly
                            auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);
                            canvasScaleY = canvasRect.scaleY;
                        } else {
                            // Child of another UI element - get canvas scale from canvas
                            auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);
                            canvasScaleY = canvasRect.scaleY;
                        }
                        
                        // Calculate in world units first (relative to canvas pivot)
                        float anchorMinYRelativeToPivot = (rect.anchorMinY - parentPivotY) * parentHeight;
                        float anchorMaxYRelativeToPivot = (rect.anchorMaxY - parentPivotY) * parentHeight;
                        
                        // Determine which is bottom and which is top
                        float bottomAnchorPos = std::min(anchorMinYRelativeToPivot, anchorMaxYRelativeToPivot);
                        float topAnchorPos = std::max(anchorMinYRelativeToPivot, anchorMaxYRelativeToPivot);
                        
                        // offsetMinY = bottom offset (positive = padding inward from bottom)
                        // offsetMaxY = top offset (positive = padding inward from top)
                        float bottomEdge = bottomAnchorPos + rect.offsetMinY;
                        float topEdge = topAnchorPos - rect.offsetMaxY;
                        float calculatedHeightWorld = topEdge - bottomEdge;
                        
                        // Ensure height is positive
                        if (calculatedHeightWorld < 0.0f) {
                            std::swap(bottomEdge, topEdge);
                            calculatedHeightWorld = -calculatedHeightWorld;
                        }
                        
                        // Convert from world units to UI pixels (divide by canvas scale)
                        // This ensures calculatedHeight is in the same units as rect.height (UI pixels)
                        // Example: canvas height = 10 world units, canvas scale = 0.1
                        // calculatedHeightWorld = 10, calculatedHeight = 10 / 0.1 = 100 pixels
                        // When rendered: 100 pixels * 0.1 scale = 10 world units (fills canvas)
                        float calculatedHeight = calculatedHeightWorld / canvasScaleY;
                        
                        // Position is the pivot position (in world units)
                        // In Y-up: pivotY=0 is bottom, pivotY=1 is top
                        float pivotY = bottomEdge + calculatedHeightWorld * rect.pivotY;
                        result.posY += pivotY;
                        result.calculatedHeight = calculatedHeight;
                    }
                    else {
                        // Point anchor: position is anchor relative to parent pivot + offset
                        // In Y-up: anchorMinY=0 is bottom, anchorMinY=1 is top
                        float anchorYRelativeToPivot = (rect.anchorMinY - parentPivotY) * parentHeight;
                        result.posY += anchorYRelativeToPivot + rect.y;
                    }

                    result.posZ += rect.z;
                }
                else {
                    // For parent entities, accumulate position and apply anchor offset relative to their parent pivot
                    float anchorXRelativeToPivot = (rect.anchorMinX - parentPivotX) * parentWidth;
                    float anchorYRelativeToPivot = (rect.anchorMinY - parentPivotY) * parentHeight;
                    result.posX += anchorXRelativeToPivot + rect.x;
                    result.posY += anchorYRelativeToPivot + rect.y;
                    result.posZ += rect.z;
                }
            }

            return result;
        }

        //=====================================================================
        // SCREEN SPACE: Apply center anchor and scaleFactor
        // Canvas transform is IGNORED (locked like Unity)
        //=====================================================================
        result.scaleX = canvas.scaleFactor;
        result.scaleY = canvas.scaleFactor;

        std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

        // Special case: If querying the Canvas itself (root entity, no parent)
        // For Canvas, rect.x/y already represents the pivot position in screen space
        // We should NOT add pivot offset again - the position IS the pivot position
        // This is because Canvas has no parent anchor to offset from
        if (entity == canvasEntity) {
            auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);
            // Canvas position (rect.x, rect.y) is already the pivot position
            result.posX = rect.x * result.scaleX;
            result.posY = rect.y * result.scaleY;
            result.posZ = rect.z;
            return result;
        }

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity current = chain[i];
            auto& rect = m_cm->GetComponent<Component::UIRectTransform>(current);
            bool isTarget = (current == entity);

            result.scaleX *= rect.scaleX;
            result.scaleY *= rect.scaleY;
            result.scaleZ *= rect.scaleZ;
            result.rotationZ += rect.rotationZ;

            // Get parent dimensions for anchor calculation
            float parentWidth = 0.f;
            float parentHeight = 0.f;

            if (i > 0) {
                Entity parentEntity = chain[i - 1];
                auto& parentRect = m_cm->GetComponent<Component::UIRectTransform>(parentEntity);
                parentWidth = parentRect.width;
                parentHeight = parentRect.height;
            }
            else {
                // Direct child of canvas - use screen dimensions in reference coordinates
                parentWidth = NE::Graphics::GraphicsManager::GetScreenWidth() / canvas.scaleFactor;
                parentHeight = NE::Graphics::GraphicsManager::GetScreenHeight() / canvas.scaleFactor;
            }

            // Calculate anchor position using actual anchor values from component
            bool isStretchedX = rect.IsStretchedX();
            bool isStretchedY = rect.IsStretchedY();

            if (isTarget) {
                // For the target entity, calculate position and size based on anchor mode
                if (isStretchedX) {
                    // Stretched horizontally: edges are anchored, size calculated from anchor spread
                    float anchorMinX = parentWidth * rect.anchorMinX;
                    float anchorMaxX = parentWidth * rect.anchorMaxX;
                    float leftEdge = anchorMinX + rect.offsetMinX;
                    float rightEdge = anchorMaxX - rect.offsetMaxX;
                    float calculatedWidth = rightEdge - leftEdge;
                    
                    // Position is the left edge (or pivot position if we need it)
                    // For stretched anchors, we store the pivot position
                    float pivotX = leftEdge + calculatedWidth * rect.pivotX;
                    result.posX += pivotX;
                    result.calculatedWidth = calculatedWidth;
                }
                else {
                    // Point anchor: position is anchor + offset
                    float anchorX = parentWidth * rect.anchorMinX;
                    result.posX += anchorX + rect.x;
                }

                if (isStretchedY) {
                    // Stretched vertically: edges are anchored, size calculated from anchor spread
                    // Unity: anchorMinY/anchorMaxY are normalized (0.0=bottom, 1.0=top in Unity's Y-up)
                    // In top-left origin: 0.0=top, parentHeight=bottom (Y-down)
                    // Convert: Unity bottom (0.0) → our bottom (parentHeight), Unity top (1.0) → our top (0.0)
                    float anchorMinYPos = parentHeight * (1.0f - rect.anchorMinY);  // Convert to top-left origin
                    float anchorMaxYPos = parentHeight * (1.0f - rect.anchorMaxY);  // Convert to top-left origin
                    
                    // Determine which is top and which is bottom (smaller Y = top, larger Y = bottom)
                    float topAnchorPos = std::min(anchorMinYPos, anchorMaxYPos);
                    float bottomAnchorPos = std::max(anchorMinYPos, anchorMaxYPos);
                    
                    // offsetMaxY = top offset (positive = padding inward from top)
                    // offsetMinY = bottom offset (positive = padding inward from bottom)
                    float topEdge = topAnchorPos + rect.offsetMaxY;      // Top edge: anchor + top offset
                    float bottomEdge = bottomAnchorPos - rect.offsetMinY; // Bottom edge: anchor - bottom offset
                    float calculatedHeight = bottomEdge - topEdge;
                    
                    // Position is the pivot position
                    // For stretched anchors, we store the pivot position
                    float pivotY = topEdge + calculatedHeight * (1.0f - rect.pivotY);
                    result.posY += pivotY;
                    result.calculatedHeight = calculatedHeight;
                }
                else {
                    // Point anchor: position is anchor + offset
                    float anchorY = parentHeight * (1.0f - rect.anchorMinY);
                    result.posY += anchorY + rect.y;
                }

                result.posZ += rect.z;
            }
            else {
                // For parent entities, use anchor point for positioning
                float anchorX = parentWidth * rect.anchorMinX;
                float anchorY = parentHeight * (1.0f - rect.anchorMinY);
                result.posX += anchorX + rect.x;
                result.posY += anchorY + rect.y;
                result.posZ += rect.z;
            }
        }

        return result;
    }

    //=========================================================================
    // World Transform Calculation
    //=========================================================================

    UITransformSystem::WorldTransform UITransformSystem::CalculateWorldTransform(
        Entity entity,
        Entity canvasEntity,
        const Component::UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        WorldTransform result;

        if (!m_cm->HasComponent<Component::UIRectTransform>(entity)) {
            return result;
        }

        auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);
        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);

        result.x = accumulated.posX;
        result.y = accumulated.posY;
        result.z = accumulated.posZ;
        
        // Use calculated width/height if anchors are stretched, otherwise use rect.width/height
        if (accumulated.calculatedWidth > 0.f) {
            result.width = accumulated.calculatedWidth * accumulated.scaleX;
        }
        else {
            result.width = rect.width * accumulated.scaleX;
        }
        
        if (accumulated.calculatedHeight > 0.f) {
            result.height = accumulated.calculatedHeight * accumulated.scaleY;
        }
        else {
            result.height = rect.height * accumulated.scaleY;
        }
        result.accumulatedRotationZ = accumulated.rotationZ;
        result.accumulatedScaleX = accumulated.scaleX;
        result.accumulatedScaleY = accumulated.scaleY;

        // Pixel-perfect snapping for screen space only
        if (canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
            canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
            if (canvas.pixelPerfect) {
                ApplyPixelPerfectSnapping(result);
            }
        }

        return result;
    }

    void UITransformSystem::ApplyPixelPerfectSnapping(WorldTransform& transform) {
        transform.x = std::round(transform.x);
        transform.y = std::round(transform.y);
        transform.width = std::round(transform.width);
        transform.height = std::round(transform.height);
    }

    //=========================================================================
    // Model Matrix Building
    //=========================================================================

    Math::Mat4 UITransformSystem::BuildWorldSpaceModelMatrix(
        Entity entity,
        Entity canvasEntity,
        const Component::UIRectTransform& rect,
        const AccumulatedTransform& accumulated
    ) {
        // World space vertices are generated as a unit quad from (0,0) to (1,1) in Y-down space
        // We need to transform this to world space (Y-up) with proper pivot handling
        //
        // Steps:
        // 1. Scale the unit quad to desired size
        // 2. Apply pivot offset (accounting for Y-axis flip from Y-down to Y-up)
        // 3. Rotate
        // 4. Translate to world position
        
        Math::Vec2 pivot = rect.GetPivot();
        
        // Use calculated width/height if anchors are stretched, otherwise use rect.width/height
        float baseWidth = (accumulated.calculatedWidth > 0.f) ? accumulated.calculatedWidth : rect.width;
        float baseHeight = (accumulated.calculatedHeight > 0.f) ? accumulated.calculatedHeight : rect.height;
        
        float scaledWidth = baseWidth * accumulated.scaleX;
        float scaledHeight = baseHeight * accumulated.scaleY;
        
        // Step 1: Scale the unit quad (0,0 to 1,1) to (0,0 to width, height)
        Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
            scaledWidth,
            scaledHeight,
            accumulated.scaleZ
        );
        
        // Step 2: Apply pivot offset
        // After scaling, the quad is from (0,0) to (width, height) in Y-down space
        // In Y-down space: (0,0) = top-left, (1,1) = bottom-right
        // In Y-up world space: (0,0) = bottom-left, (1,1) = top-right
        // Unity pivot: (0,0) = bottom-left, (0.5,0.5) = center, (1,1) = top-right
        //
        // The pivot point in the scaled quad (Y-down) is at:
        //   X: width * pivotX  (pivotX=0 → left, pivotX=1 → right) ✓
        //   Y: height * pivotY (pivotY=0 → top in Y-down, but should be bottom in Y-up)
        //                      (pivotY=1 → bottom in Y-down, but should be top in Y-up)
        //
        // To convert Y-down to Y-up: Y_world = height - Y_ui
        // So pivotY_world = height - (height * pivotY) = height * (1 - pivotY)
        //
        // To move the pivot to origin, translate by:
        //   X: -width * pivotX
        //   Y: -height * (1 - pivotY)  (flipped for Y-up)
        float pivotOffsetX = -scaledWidth * pivot.x;
        float pivotOffsetY = -scaledHeight * (1.0f - pivot.y);  // Flip Y for world space
        
        Math::Mat4 pivotMatrix = Math::Mat4::BuildTranslation(
            pivotOffsetX,
            pivotOffsetY,
            0.0f
        );

        // Step 3: Rotation
        // Rotation order: X * Y * Z (same as TransformSystem)
        // Note: Rotation matrices work with any angle value (they're periodic), so we don't need to normalize
        // The normalization is only for display/storage of Euler angles, not for matrix building
        Math::Mat4 rotationX = Math::Mat4::BuildXRotation(accumulated.rotationX * PI / 180.0f);
        Math::Mat4 rotationY = Math::Mat4::BuildYRotation(accumulated.rotationY * PI / 180.0f);
        Math::Mat4 rotationZ = Math::Mat4::BuildZRotation(accumulated.rotationZ * PI / 180.0f);
        Math::Mat4 rotationMatrix = rotationX * rotationY * rotationZ;  // X * Y * Z order (same as TransformSystem)

        // Step 4: Translation to world position (accumulated.posX/Y/Z is the pivot position)
        Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(
            accumulated.posX,
            accumulated.posY,
            accumulated.posZ
        );

        // Order: Translate -> Rotate -> Pivot Offset -> Scale
        // This means: first scale, then move pivot to origin, then rotate, then move to world position
        return translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
    }

    //=========================================================================
    // Canvas Setup
    //=========================================================================

    void UITransformSystem::SetupCanvasDefaults(Entity canvasEntity, Component::UICanvas& canvas) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(canvasEntity)) {
            return;
        }

        // Check if render mode changed or first time setup
        bool renderModeChanged = canvas.hasBeenInitialized &&
            (canvas.renderMode != canvas.lastInitializedMode);

        if (canvas.hasBeenInitialized && !renderModeChanged) {
            // Already initialized and mode hasn't changed - only update screen space position
            if (canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
                canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA)
            {
                auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);
                float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
                float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
                canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
                canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
                canvasRect.width = canvas.referenceWidth;
                canvasRect.height = canvas.referenceHeight;
            }
            return;
        }

        // First time OR render mode changed - apply full defaults
        auto& canvasRect = m_cm->GetComponent<Component::UIRectTransform>(canvasEntity);

        switch (canvas.renderMode) {
        case Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY:
        case Component::UICanvas::RenderMode::SCREEN_SPACE_CAMERA: {
            float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

            canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
            canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
            canvasRect.z = 0.f;

            // Reset scale to 1 (scaleFactor handles scaling)
            canvasRect.scaleX = 1.f;
            canvasRect.scaleY = 1.f;
            canvasRect.scaleZ = 1.f;

            canvasRect.width = canvas.referenceWidth;
            canvasRect.height = canvas.referenceHeight;

            // Reset rotation
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }

        case Component::UICanvas::RenderMode::WORLD_SPACE: {
            // Apply default world space scale (Unity default: 0.01)
            canvasRect.scaleX = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleY = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleZ = 1.f;

            // Reset position to origin (user can move it)
            canvasRect.x = 0.f;
            canvasRect.y = 0.f;
            canvasRect.z = -5.f;  // Place in front of camera

            // Set default canvas size in world units (Unity defaults to 1x1 world units)
            // This represents the UI coordinate space, not the pixel size
            if (!canvas.hasBeenInitialized || renderModeChanged) {
                canvasRect.width = DEFAULT_WORLD_SPACE_CANVAS_WIDTH;
                canvasRect.height = DEFAULT_WORLD_SPACE_CANVAS_HEIGHT;
            }

            // Keep rotation as is or reset
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }
        }

        // Mark as initialized with current mode
        canvas.hasBeenInitialized = true;
        canvas.lastInitializedMode = canvas.renderMode;
    }

    //=========================================================================
    // Canvas & Scaling
    //=========================================================================

    float UITransformSystem::CalculateScaleFactor(const Component::UICanvas& canvas) {
        float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        switch (canvas.scaleMode) {
        case Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE: {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;
            return std::min(widthScale, heightScale);
        }

        case Component::UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE: {
            return 1.0f;
        }

        case Component::UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE: {
            float referenceDPI = 96.0f;
            float currentDPI = 96.0f;
            return currentDPI / referenceDPI;
        }
        }

        return 1.0f;
    }

} // namespace NE::ECS::Systems