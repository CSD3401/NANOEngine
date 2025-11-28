#ifndef UI_LAYOUT_CALCULATOR_HPP
#define UI_LAYOUT_CALCULATOR_HPP

#include "Math/Vec2.hpp"
#include "../ECS/Components/UIRectTransform.hpp"

namespace NE::UI {

    /**
     * Calculated layout result after applying anchors and pivot
     */
    struct LayoutResult {
        float x;        // Final screen X position (top-left of element)
        float y;        // Final screen Y position (top-left of element)
        float width;    // Final width
        float height;   // Final height
    };

    /**
     * Calculate the final screen position and size of a UI element
     * based on its anchors, pivot, and parent dimensions.
     *
     * @param rect          The UIRectTransform component
     * @param parentX       Parent's top-left X position
     * @param parentY       Parent's top-left Y position
     * @param parentWidth   Parent's width
     * @param parentHeight  Parent's height
     * @return              Final screen position and size
     */
    inline LayoutResult CalculateLayout(
        const NE::ECS::Component::UIRectTransform& rect,
        float parentX,
        float parentY,
        float parentWidth,
        float parentHeight
    ) {
        LayoutResult result;

        // Step 1: Calculate anchor positions in parent space
        // Anchors are normalized (0-1), so we multiply by parent size
        float anchorMinXPos = parentX + rect.anchorMinX * parentWidth;
        float anchorMinYPos = parentY + rect.anchorMinY * parentHeight;
        float anchorMaxXPos = parentX + rect.anchorMaxX * parentWidth;
        float anchorMaxYPos = parentY + rect.anchorMaxY * parentHeight;

        // Step 2: Check if anchors are the same (single point) or different (stretched)
        bool isStretchedX = (rect.anchorMinX != rect.anchorMaxX);
        bool isStretchedY = (rect.anchorMinY != rect.anchorMaxY);

        // Step 3: Calculate final size
        if (isStretchedX) {
            // Width is determined by anchor spread, with rect.x and rect.width acting as padding/offsets
            // rect.x = left padding from anchorMin
            // rect.width = negative right padding from anchorMax (or you can use a separate "right" offset)
            // For simplicity: width = anchor spread - left offset - right offset
            // Unity uses "left", "right" instead of x/width when stretched
            // Here we'll use: x = left offset, width = right offset (both inward)
            result.width = (anchorMaxXPos - anchorMinXPos) - rect.x - rect.width;
            if (result.width < 0) result.width = 0;
        }
        else {
            // Fixed size - width is exactly what's specified
            result.width = rect.width;
        }

        if (isStretchedY) {
            // Same logic for height
            // rect.y = bottom offset, rect.height = top offset
            result.height = (anchorMaxYPos - anchorMinYPos) - rect.y - rect.height;
            if (result.height < 0) result.height = 0;
        }
        else {
            // Fixed size
            result.height = rect.height;
        }

        // Step 4: Calculate position based on pivot
        // The pivot determines which point of the element is at the anchor position
        if (isStretchedX) {
            // When stretched, x/width are offsets from anchors
            result.x = anchorMinXPos + rect.x;
        }
        else {
            // When not stretched, position is relative to anchor point
            // rect.x is the offset from anchor, then we subtract pivot offset
            float anchorX = anchorMinXPos; // Single anchor point
            result.x = anchorX + rect.x - (rect.pivotX * result.width);
        }

        if (isStretchedY) {
            result.y = anchorMinYPos + rect.y;
        }
        else {
            float anchorY = anchorMinYPos;
            result.y = anchorY + rect.y - (rect.pivotY * result.height);
        }

        return result;
    }

    /**
     * Simplified version for single-point anchors (most common case)
     * This is easier to understand and matches Unity's behavior better
     */
    inline LayoutResult CalculateLayoutSimple(
        const NE::ECS::Component::UIRectTransform& rect,
        float parentX,
        float parentY,
        float parentWidth,
        float parentHeight
    ) {
        LayoutResult result;
        result.width = rect.width;
        result.height = rect.height;

        // Calculate where the anchor point is in screen space
        float anchorX = parentX + rect.anchorMinX * parentWidth;
        float anchorY = parentY + rect.anchorMinY * parentHeight;

        // rect.x and rect.y are offsets from the anchor point
        // But they're relative to the pivot, not the top-left corner
        // So: final_position = anchor + offset - (pivot * size)

        result.x = anchorX + rect.x - (rect.pivotX * rect.width);
        result.y = anchorY + rect.y - (rect.pivotY * rect.height);

        return result;
    }

} // namespace NE::UI

#endif // UI_LAYOUT_CALCULATOR_HPP
