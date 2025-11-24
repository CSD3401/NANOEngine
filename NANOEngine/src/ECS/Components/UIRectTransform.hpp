#ifndef UI_RECT_TRANSFORM_HPP
#define UI_RECT_TRANSFORM_HPP

#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"

namespace NE::ECS::Component {

    struct UIRectTransform {
        std::string luid;

        // top-left position in pixels
        float x = 0.0f;
        float y = 0.0f;
        float z = -300.0f;

        // size in pixels
        float width = 100.0f;
        float height = 100.0f;

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

        uint32_t parent = 0;

        // helper function: get rotation matrix from euler angles
        NE::Math::Mat4 GetRotationMatrix() const {
            // combine XYZ rotations: Rz * Ry * Rx (ZYX order)
            NE::Math::Mat4 rotX = NE::Math::Mat4::BuildXRotation(rotationX);
            NE::Math::Mat4 rotY = NE::Math::Mat4::BuildYRotation(rotationY);
            NE::Math::Mat4 rotZ = NE::Math::Mat4::BuildZRotation(rotationZ);

            // apply in ZYX order (common for Euler angles)
            return rotZ * rotY * rotX;
        }

        // get scale as Vec3
        NE::Math::Vec3 GetScale() const {
            return NE::Math::Vec3(scaleX, scaleY, scaleZ);
        }

        // get position as Vec3
        NE::Math::Vec3 GetPosition() const {
            return NE::Math::Vec3(x, y, z);
        }

        // get pivot as Vec2
        NE::Math::Vec2 GetPivot() const {
            return NE::Math::Vec2(pivotX, pivotY);
        }

        // get anchor min as Vec2
        NE::Math::Vec2 GetAnchorMin() const {
            return NE::Math::Vec2(anchorMinX, anchorMinY);
        }

        // get anchor max as Vec2
        NE::Math::Vec2 GetAnchorMax() const {
            return NE::Math::Vec2(anchorMaxX, anchorMaxY);
        }
    };

} // namespace NE::ECS::Component
#endif // END UI_RECT_TRANSFORM_HPP
