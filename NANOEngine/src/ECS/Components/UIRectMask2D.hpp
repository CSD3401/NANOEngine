#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIRectMask2D {
        bool enabled = true;

        float paddingLeft = 0.f;
        float paddingRight = 0.f;
        float paddingTop = 0.f;
        float paddingBottom = 0.f;

        NE_REFLECT_BEGIN(UIRectMask2D)
            NE_REFLECT_FIELD(enabled),
            NE_REFLECT_FIELD(paddingLeft),
            NE_REFLECT_FIELD(paddingRight),
            NE_REFLECT_FIELD(paddingTop),
            NE_REFLECT_FIELD(paddingBottom)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
