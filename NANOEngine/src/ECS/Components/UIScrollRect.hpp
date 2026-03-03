#pragma once

#include "../../Core/Reflection.hpp"
#include "../../Math/Vec2.hpp"
#include <cstdint>

namespace NE::ECS::Component {

    struct UIScrollRect {
        enum class MovementType { Unrestricted = 0, Elastic = 1, Clamped = 2 };

        // Entity references
        uint32_t contentEntity = UINT32_MAX;
        uint32_t viewportEntity = UINT32_MAX;
        uint32_t horizontalScrollbar = UINT32_MAX;
        uint32_t verticalScrollbar = UINT32_MAX;

        bool horizontal = true;
        bool vertical = true;

        MovementType movementType = MovementType::Elastic;

        float elasticity = 0.1f;
        bool inertia = true;
        float decelerationRate = 0.135f;
        float scrollSensitivity = 1.0f;
        bool interactable = true;

        NE_REFLECT_BEGIN(UIScrollRect)
            NE_REFLECT_FIELD(contentEntity),
            NE_REFLECT_FIELD(viewportEntity),
            NE_REFLECT_FIELD(horizontalScrollbar),
            NE_REFLECT_FIELD(verticalScrollbar),
            NE_REFLECT_FIELD(horizontal),
            NE_REFLECT_FIELD(vertical),
            NE_REFLECT_FIELD(movementType),
            NE_REFLECT_FIELD(elasticity),
            NE_REFLECT_FIELD(inertia),
            NE_REFLECT_FIELD(decelerationRate),
            NE_REFLECT_FIELD(scrollSensitivity),
            NE_REFLECT_FIELD(interactable)
        NE_REFLECT_END()

        // Runtime state (not serialized)
        NE::Math::Vec2 velocity{ 0.f, 0.f };
        NE::Math::Vec2 normalizedPosition{ 0.f, 0.f };
        bool isDragging = false;
        float dragStartX = 0.f;
        float dragStartY = 0.f;
        float contentStartX = 0.f;
        float contentStartY = 0.f;
        float contentWidth = 0.f;
        float contentHeight = 0.f;
        float viewportWidth = 0.f;
        float viewportHeight = 0.f;
    };

} // namespace NE::ECS::Component
