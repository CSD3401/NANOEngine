#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIVerticalLayoutGroup {
        float paddingLeft = 0.f;
        float paddingRight = 0.f;
        float paddingTop = 0.f;
        float paddingBottom = 0.f;
        float spacing = 0.f;

        // 0-8 matching Unity's TextAnchor grid:
        // 0=UpperLeft, 1=UpperCenter, 2=UpperRight
        // 3=MiddleLeft, 4=MiddleCenter, 5=MiddleRight
        // 6=LowerLeft, 7=LowerCenter, 8=LowerRight
        int childAlignment = 1; // UpperCenter for vertical

        bool controlChildWidth = true;
        bool controlChildHeight = true;
        bool childForceExpandWidth = true;
        bool childForceExpandHeight = true;
        bool reverseArrangement = false;

        NE_REFLECT_BEGIN(UIVerticalLayoutGroup)
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
