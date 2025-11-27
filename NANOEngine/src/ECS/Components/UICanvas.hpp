#ifndef UI_CANVAS_HPP
#define UI_CANVAS_HPP

#include "../../Math/Vec2.hpp"

namespace NE::ECS::Component {

    struct UICanvas {
        std::string luid;

        enum class RenderMode {
            SCREEN_SPACE_OVERLAY, // Always on top, no camera needed <--
            SCREEN_SPACE_CAMERA,  // Rendered by specific camera
            WORLD_SPACE           // Exists in 3D world <--
        };

        RenderMode renderMode = RenderMode::SCREEN_SPACE_OVERLAY; // default

        // for Camera mode
        float planeDistance = 100.0f;        // Distance from camera

        enum class ScaleMode {
            CONSTANT_PIXEL_SIZE,
            SCALE_WITH_SCREEN_SIZE, // <--
            CONSTANT_PHYSICAL_SIZE
        };

        ScaleMode scaleMode = ScaleMode::SCALE_WITH_SCREEN_SIZE; // default

        float scaleFactor = 1.0f; // default
        float referenceWidth = 1920.0f;
        float referenceHeight = 1080.0f;

        bool pixelPerfect = false;

        // other fields
        int sortingOrder = 0; // Higher values render on top (layering of canvases) (for ScreenSpaceOverlay and ScreenSpaceCamera)
        bool isActive = true;
    };

} // namespace NE::ECS::Component
#endif // END UI_CANVAS_HPP
