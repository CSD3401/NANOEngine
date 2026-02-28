#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIAutoSize {
        // === Content Size Fitter ===
        // 0=Unconstrained, 1=MinSize, 2=PreferredSize
        int horizontalFit = 0;
        int verticalFit = 0;

        // === Aspect Ratio Fitter ===
        float aspectRatio = 1.0f;  // width / height

        // 0=None, 1=WidthControlsHeight, 2=HeightControlsWidth,
        // 3=FitInParent, 4=EnvelopeParent
        int aspectMode = 0;

        NE_REFLECT_BEGIN(UIAutoSize)
            NE_REFLECT_FIELD(horizontalFit),
            NE_REFLECT_FIELD(verticalFit),
            NE_REFLECT_FIELD(aspectRatio),
            NE_REFLECT_FIELD(aspectMode)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
