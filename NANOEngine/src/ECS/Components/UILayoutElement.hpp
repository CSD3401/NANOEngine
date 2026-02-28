#pragma once

#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UILayoutElement {
        bool ignoreLayout = false;

        // -1 means "use rect dimensions"
        float minWidth = -1.f;
        float minHeight = -1.f;
        float preferredWidth = -1.f;
        float preferredHeight = -1.f;
        float flexibleWidth = -1.f;
        float flexibleHeight = -1.f;

        NE_REFLECT_BEGIN(UILayoutElement)
            NE_REFLECT_FIELD(ignoreLayout),
            NE_REFLECT_FIELD(minWidth),
            NE_REFLECT_FIELD(minHeight),
            NE_REFLECT_FIELD(preferredWidth),
            NE_REFLECT_FIELD(preferredHeight),
            NE_REFLECT_FIELD(flexibleWidth),
            NE_REFLECT_FIELD(flexibleHeight)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
