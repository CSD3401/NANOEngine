#ifndef UI_RECT_TRANSFORM_HPP
#define UI_RECT_TRANSFORM_HPP

#include "../../Math/Vec2.hpp"

namespace NE::ECS::Component {

    class UIRectTransform {
    public:
        // top-left position in pixels
        float x = 0.0f;
        float y = 0.0f;

        // size in pixels
        float width = 100.0f;
        float height = 40.0f;

        // later you can add anchor, pivot, rotation
    };

} // namespace NE::ECS::Component
#endif // END GIZMO_RENDERER_HPP
