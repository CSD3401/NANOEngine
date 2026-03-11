#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIAutoSize {
        enum class FitMode   { Unconstrained = 0, MinSize = 1, PreferredSize = 2 };
        enum class AspectMode {
            None               = 0,
            WidthControlsHeight = 1,
            HeightControlsWidth = 2,
            FitInParent        = 3,
            EnvelopeParent     = 4
        };

        // === Content Size Fitter ===
        FitMode horizontalFit = FitMode::Unconstrained;
        FitMode verticalFit   = FitMode::Unconstrained;

        // === Aspect Ratio Fitter ===
        float aspectRatio = 1.0f;  // width / height
        AspectMode aspectMode = AspectMode::None;

        NE_REFLECT_BEGIN(UIAutoSize)
            NE_REFLECT_FIELD(horizontalFit),
            NE_REFLECT_FIELD(verticalFit),
            NE_REFLECT_FIELD(aspectRatio),
            NE_REFLECT_FIELD(aspectMode)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
