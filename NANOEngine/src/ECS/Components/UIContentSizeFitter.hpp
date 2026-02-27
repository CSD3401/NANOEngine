#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIContentSizeFitter {
        // 0=Unconstrained, 1=MinSize, 2=PreferredSize
        int horizontalFit = 0;
        int verticalFit = 0;

        NE_REFLECT_BEGIN(UIContentSizeFitter)
            NE_REFLECT_FIELD(horizontalFit),
            NE_REFLECT_FIELD(verticalFit)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
