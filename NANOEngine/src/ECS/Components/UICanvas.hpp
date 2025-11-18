#ifndef UI_CANVAS_HPP
#define UI_CANVAS_HPP

#include "../../Math/Vec2.hpp"

namespace NE::ECS::Component {

    struct UICanvas {

        enum class RenderMode {
            SCREEN_SPACE_OVERLAY, // Always on top, no camera needed <--
            SCREEN_SPACE_CAMERA,  // Rendered by specific camera
            WORLD_SPACE           // Exists in 3D world <--
        };

        enum class ScaleMode {
            CONSTANT_PIXEL_SIZE,
            SCALE_WITH_SCREEN_SIZE, // <--
            CONSTANT_PHYSICAL_SIZE
        };

        RenderMode renderMode = RenderMode::SCREEN_SPACE_OVERLAY; // default

        ScaleMode scaleMode = ScaleMode::SCALE_WITH_SCREEN_SIZE; // default

        // For ScreenSpaceOverlay and ScreenSpaceCamera
        int sortingOrder = 0;  // Higher values render on top (layering of canvases)

        // Reference resolution for scaling
        float referenceWidth = 1920.0f;
        float referenceHeight = 1080.0f;

        // Scaling factor (calculated at runtime)
        float scaleFactor = 1.0f; // default

        // Is this canvas active?
        bool isActive = true;
    };

} // namespace NE::ECS::Component
#endif // END UI_CANVAS_HPP
