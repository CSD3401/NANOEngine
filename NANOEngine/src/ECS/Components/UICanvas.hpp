#ifndef UI_CANVAS_HPP
#define UI_CANVAS_HPP

#include <string>
#include "../../Math/Vec2.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UICanvas {
        enum class RenderMode {
            SCREEN_SPACE_OVERLAY, // Always on top, no camera needed
            SCREEN_SPACE_CAMERA,  // Rendered by specific camera
            WORLD_SPACE           // Exists in 3D world
        };

        enum class ScaleMode {
            CONSTANT_PIXEL_SIZE,
            SCALE_WITH_SCREEN_SIZE,
            CONSTANT_PHYSICAL_SIZE
        };

        // LUID for serialization
        uint64_t luid = 0;

        RenderMode renderMode = RenderMode::SCREEN_SPACE_OVERLAY;
        ScaleMode scaleMode = ScaleMode::SCALE_WITH_SCREEN_SIZE;

        // For Camera / World Space mode
        uint32_t cameraEntity = UINT32_MAX;  // Entity with Camera component (UINT32_MAX = use editor/main camera)
        float planeDistance = 100.0f;  // Distance from camera

        float referenceWidth = 1920.0f;
        float referenceHeight = 1080.0f;

        bool pixelPerfect = false;
        bool isActive = true;

        // Higher values render on top (layering of canvases)
        int sortingOrder = 0;

        // Canvas-level opacity multiplied into all child element colors (0 = invisible, 1 = fully opaque)
        float alpha = 1.0f;

        // Reflection
        NE_REFLECT_BEGIN(UICanvas)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(renderMode),
            NE_REFLECT_FIELD(scaleMode),
            NE_REFLECT_FIELD(cameraEntity),
            NE_REFLECT_FIELD(planeDistance),
            NE_REFLECT_FIELD(referenceWidth),
            NE_REFLECT_FIELD(referenceHeight),
            NE_REFLECT_FIELD(pixelPerfect),
            NE_REFLECT_FIELD(isActive),
            NE_REFLECT_FIELD(sortingOrder),
            NE_REFLECT_FIELD(alpha)
        NE_REFLECT_END()

        // run time only
        float scaleFactor = 1.0f;
        RenderMode lastInitializedMode = RenderMode::SCREEN_SPACE_OVERLAY;
        bool hasBeenInitialized = false;
    };

} // namespace NE::ECS::Component

#endif // UI_CANVAS_HPP