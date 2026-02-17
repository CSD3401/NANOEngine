#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIAspectRatioFitter {
        float aspectRatio = 1.0f;  // width / height

        // 0=None, 1=WidthControlsHeight, 2=HeightControlsWidth,
        // 3=FitInParent, 4=EnvelopeParent
        int aspectMode = 0;

        NE_REFLECT_BEGIN(UIAspectRatioFitter)
            NE_REFLECT_FIELD(aspectRatio),
            NE_REFLECT_FIELD(aspectMode)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
