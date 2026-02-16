#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIGridLayoutGroup {
        float paddingLeft = 0.f;
        float paddingRight = 0.f;
        float paddingTop = 0.f;
        float paddingBottom = 0.f;

        float cellWidth = 100.f;
        float cellHeight = 100.f;

        float spacingX = 0.f;
        float spacingY = 0.f;

        // 0=UpperLeft, 1=UpperRight, 2=LowerLeft, 3=LowerRight
        int startCorner = 0;

        // 0=Horizontal (fill rows first), 1=Vertical (fill columns first)
        int startAxis = 0;

        // 0-8 matching Unity's TextAnchor grid
        int childAlignment = 0;

        // 0=Flexible, 1=FixedColumnCount, 2=FixedRowCount
        int constraint = 0;
        int constraintCount = 2;

        NE_REFLECT_BEGIN(UIGridLayoutGroup)
            NE_REFLECT_FIELD(paddingLeft),
            NE_REFLECT_FIELD(paddingRight),
            NE_REFLECT_FIELD(paddingTop),
            NE_REFLECT_FIELD(paddingBottom),
            NE_REFLECT_FIELD(cellWidth),
            NE_REFLECT_FIELD(cellHeight),
            NE_REFLECT_FIELD(spacingX),
            NE_REFLECT_FIELD(spacingY),
            NE_REFLECT_FIELD(startCorner),
            NE_REFLECT_FIELD(startAxis),
            NE_REFLECT_FIELD(childAlignment),
            NE_REFLECT_FIELD(constraint),
            NE_REFLECT_FIELD(constraintCount)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
