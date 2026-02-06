#ifndef UI_RECT_TRANSFORM_HPP
#define UI_RECT_TRANSFORM_HPP

#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"
#include <string>

namespace NE::ECS::Component {
    //inline constexpr uint32_t INVALID_PARENT = UINT32_MAX;

    struct UIRectTransform {
        // NOTE: Parent-child relationships now managed by Hierarchy component
        // DEPRECATED: uint32_t parent, luid, parentLuid - use Hierarchy component instead
        // These fields kept temporarily for backward compatibility during migration
        uint32_t parent = UINT32_MAX;  // DEPRECATED
        uint64_t luid = 0;              // DEPRECATED (use Hierarchy::luid)
        uint64_t parentLuid = 0;        // DEPRECATED (use Hierarchy::parentLuid)

        // position of pivot point
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        // size in pixels
        float width = 100.0f;
        float height = 100.0f;

        // === OFFSETS (used when anchors are different / stretched) ===
        // Distance from anchor edges to rect edges
        // Left/Bottom are positive inward, Right/Top are negative inward
        float offsetMinX = 0.0f;  // Left offset
        float offsetMinY = 0.0f;  // Bottom offset  
        float offsetMaxX = 0.0f;  // Right offset (negative = padding inward)
        float offsetMaxY = 0.0f;  // Top offset (negative = padding inward)

        // rotation (for world space rendering)
        // store as Euler angles in degrees for easier editing
        float rotationX = 0.0f;
        float rotationY = 0.0f;
        float rotationZ = 0.0f;

        // scale (for world space rendering, 1.0 = normal size)
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;

        // anchor min/max (normalized coordinates relative to parent)
        float anchorMinX = 0.5f;
        float anchorMinY = 0.5f;
        float anchorMaxX = 0.5f;
        float anchorMaxY = 0.5f;

        // pivot (normalized coordinates, 0-1, determines rotation/scale origin)
        float pivotX = 0.5f;
        float pivotY = 0.5f;

        // Transform matrices (like Transform component)
        NE::Math::Mat4 localMatrix;           // Local TRS matrix
        NE::Math::Mat4 worldMatrix;           // World transform (accumulated from parent chain)
        bool worldMatrixDirty = true;         // Needs recalculation

        // Reflection
        NE_REFLECT_BEGIN(UIRectTransform)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(x),
            NE_REFLECT_FIELD(y),
            NE_REFLECT_FIELD(z),
            NE_REFLECT_FIELD(width),
            NE_REFLECT_FIELD(height),
            NE_REFLECT_FIELD(offsetMinX),
            NE_REFLECT_FIELD(offsetMinY),
            NE_REFLECT_FIELD(offsetMaxX),
            NE_REFLECT_FIELD(offsetMaxY),
            NE_REFLECT_FIELD(rotationX),
            NE_REFLECT_FIELD(rotationY),
            NE_REFLECT_FIELD(rotationZ),
            NE_REFLECT_FIELD(scaleX),
            NE_REFLECT_FIELD(scaleY),
            NE_REFLECT_FIELD(scaleZ),
            NE_REFLECT_FIELD(anchorMinX),
            NE_REFLECT_FIELD(anchorMinY),
            NE_REFLECT_FIELD(anchorMaxX),
            NE_REFLECT_FIELD(anchorMaxY),
            NE_REFLECT_FIELD(pivotX),
            NE_REFLECT_FIELD(pivotY),
            NE_REFLECT_FIELD_HIDDEN(parentLuid)
        NE_REFLECT_END()

        // Helper functions
        bool IsStretchedX() const { return anchorMinX != anchorMaxX; }
        bool IsStretchedY() const { return anchorMinY != anchorMaxY; }

        // Helper: get top-left corner position (calculated from pivot position)
        float GetTopLeftX() const {
            return x - width * pivotX;
        }
        float GetTopLeftY() const {
            return y - height * pivotY;
        }

        // helper function: get rotation matrix from euler angles
        NE::Math::Mat4 GetRotationMatrix() const {
            // combine XYZ rotations: Rz * Ry * Rx (ZYX order)
            NE::Math::Mat4 rotX = NE::Math::Mat4::BuildXRotation(rotationX);
            NE::Math::Mat4 rotY = NE::Math::Mat4::BuildYRotation(rotationY);
            NE::Math::Mat4 rotZ = NE::Math::Mat4::BuildZRotation(rotationZ);
            // apply in ZYX order (common for Euler angles)
            return rotZ * rotY * rotX;
        }

        NE::Math::Vec3 GetScale() const { return NE::Math::Vec3(scaleX, scaleY, scaleZ); }
        NE::Math::Vec3 GetPosition() const { return NE::Math::Vec3(x, y, z); }
        NE::Math::Vec2 GetPivot() const { return NE::Math::Vec2(pivotX, pivotY); }
        NE::Math::Vec2 GetAnchorMin() const { return NE::Math::Vec2(anchorMinX, anchorMinY); }
        NE::Math::Vec2 GetAnchorMax() const { return NE::Math::Vec2(anchorMaxX, anchorMaxY); }
    };

} // namespace NE::ECS::Component

#endif // UI_RECT_TRANSFORM_HPP