#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UILayoutGroup {
        enum class ChildAlignment {
            UpperLeft   = 0, UpperCenter  = 1, UpperRight  = 2,
            MiddleLeft  = 3, MiddleCenter = 4, MiddleRight = 5,
            LowerLeft   = 6, LowerCenter  = 7, LowerRight  = 8
        };

        bool isHorizontal = true;

        float paddingLeft = 0.f;
        float paddingRight = 0.f;
        float paddingTop = 0.f;
        float paddingBottom = 0.f;
        float spacing = 0.f;

        ChildAlignment childAlignment = ChildAlignment::MiddleLeft;

        bool controlChildWidth = true;
        bool controlChildHeight = true;
        bool childForceExpandWidth = true;
        bool childForceExpandHeight = true;
        bool reverseArrangement = false;

        NE_REFLECT_BEGIN(UILayoutGroup)
            NE_REFLECT_FIELD(isHorizontal),
            NE_REFLECT_FIELD(paddingLeft),
            NE_REFLECT_FIELD(paddingRight),
            NE_REFLECT_FIELD(paddingTop),
            NE_REFLECT_FIELD(paddingBottom),
            NE_REFLECT_FIELD(spacing),
            NE_REFLECT_FIELD(childAlignment),
            NE_REFLECT_FIELD(controlChildWidth),
            NE_REFLECT_FIELD(controlChildHeight),
            NE_REFLECT_FIELD(childForceExpandWidth),
            NE_REFLECT_FIELD(childForceExpandHeight),
            NE_REFLECT_FIELD(reverseArrangement)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
