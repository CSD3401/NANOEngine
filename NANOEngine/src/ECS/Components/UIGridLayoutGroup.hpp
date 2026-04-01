#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIGridLayoutGroup {
        enum class StartCorner  { UpperLeft = 0, UpperRight = 1, LowerLeft = 2, LowerRight = 3 };
        enum class StartAxis    { Horizontal = 0, Vertical = 1 };
        enum class ChildAlignment {
            UpperLeft   = 0, UpperCenter  = 1, UpperRight  = 2,
            MiddleLeft  = 3, MiddleCenter = 4, MiddleRight = 5,
            LowerLeft   = 6, LowerCenter  = 7, LowerRight  = 8
        };
        enum class Constraint   { Flexible = 0, FixedColumnCount = 1, FixedRowCount = 2 };

        float paddingLeft = 0.f;
        float paddingRight = 0.f;
        float paddingTop = 0.f;
        float paddingBottom = 0.f;

        float cellWidth = 100.f;
        float cellHeight = 100.f;

        float spacingX = 0.f;
        float spacingY = 0.f;

        StartCorner   startCorner     = StartCorner::UpperLeft;
        StartAxis     startAxis       = StartAxis::Horizontal;
        ChildAlignment childAlignment = ChildAlignment::UpperLeft;
        Constraint    constraint      = Constraint::Flexible;
        int           constraintCount = 2;

        // When true, cell size is computed to fill available space evenly (ignores cellWidth/cellHeight)
        bool stretchCells = false;

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
            NE_REFLECT_FIELD(constraintCount),
            NE_REFLECT_FIELD(stretchCells)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
