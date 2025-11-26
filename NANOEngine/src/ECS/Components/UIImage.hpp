#ifndef UI_IMAGE_HPP
#define UI_IMAGE_HPP

#include <string>
#include <memory>
#include "../../Graphics/Core/Material.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::ECS::Component {

    struct UIImage {
        std::string luid;
        std::string textureUUID;
        std::shared_ptr<NE::Graphics::Material> material;
        NE::Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f }; // tint
        int renderMode = 0;
        bool isDirty = false;

        // image type
        enum class ImageType {
            SIMPLE,         // standard image, no special behavior
            SLICED,         // 9-slice scaling (borders stay same size, center stretches)
            TILED,          // texture repeats to fill area
            FILLED          // fills based on a value (radial, horizontal, vertical, etc.)
        };
        ImageType imageType = ImageType::SIMPLE;

        // fill type (for filled image type)
        enum class FillMethod {
            HORIZONTAL,     // fill left-to-right or right-to-left
            VERTICAL,       // fill bottom-to-top or top-to-bottom
            RADIAL_90,      // fill in 90 degree arc
            RADIAL_180,     // fill in 180 degree arc
            RADIAL_360      // fill in full circle
        };
        FillMethod fillMethod = FillMethod::HORIZONTAL;

        // fill amount (0.0 = empty, 1.0 = full)
        float fillAmount = 1.0f; 

        // fill origin/direction
        enum class FillOrigin {
            // for horizontal
            LEFT = 0,
            RIGHT = 1,

            // for vertical
            BOTTOM = 0,
            TOP = 1,

            // for radial
            BOTTOM_RADIAL = 0,
            RIGHT_RADIAL = 1,
            TOP_RADIAL = 2,
            LEFT_RADIAL = 3
        };
        FillOrigin fillOrigin = FillOrigin::LEFT;

        // fill clockwise (for radial fills)
        bool fillClockwise = true;

        // sliced image settings (for sliced image type)
        // border sizes in pixels (for 9-slice)
        float borderLeft = 0.0f;
        float borderRight = 0.0f;
        float borderTop = 0.0f;
        float borderBottom = 0.0f;

        // pixels per unit multiplier (for tiled image type)
        float pixelsPerUnitMultiplier = 1.0f;

        // preserve aspect ratio when scaling (for image type)
        bool preserveAspect = false;

        // helper functions
        // check if image needs special rendering
        bool RequiresCustomRendering() const {
            return imageType != ImageType::SIMPLE || fillAmount < 1.0f;
        }

        // get effective fill origin as integer for easier handling
        int GetFillOriginValue() const {
            return static_cast<int>(fillOrigin);
        }

        // validate fill amount
        void ClampFillAmount() {
            if (fillAmount < 0.0f) fillAmount = 0.0f;
            if (fillAmount > 1.0f) fillAmount = 1.0f;
        }
    };

} // namespace NE::ECS::Component
#endif // END UI_IMAGE_HPP