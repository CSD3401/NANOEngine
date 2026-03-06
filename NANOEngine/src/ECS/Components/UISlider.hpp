#ifndef UI_SLIDER_HPP
#define UI_SLIDER_HPP

#include "Core/Reflection.hpp"
#include <cstdint>

namespace NE::ECS::Component {

    struct UISlider {
        // Direction of the slider
        enum class Direction {
            LEFT_TO_RIGHT,
            RIGHT_TO_LEFT,
            BOTTOM_TO_TOP,
            TOP_TO_BOTTOM
        };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;

        float value = 0.0f;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool wholeNumbers = false;
        Direction direction = Direction::LEFT_TO_RIGHT;

        // Child entity references (set during creation)
        uint32_t fillRect = UINT32_MAX;     // The fill image entity
        uint32_t handleRect = UINT32_MAX;   // The draggable handle entity
        uint32_t backgroundRect = UINT32_MAX; // The background image entity

        bool interactable = true;

        // === RUNTIME-ONLY FIELDS (not serialized) ===
        bool isDragging = false;
        bool valueChanged = false;  // True for one frame when value changes (for script polling)
        float previousValue = 0.0f; // For detecting changes

        // Reflection
        NE_REFLECT_BEGIN(UISlider)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(value),
            NE_REFLECT_FIELD(minValue),
            NE_REFLECT_FIELD(maxValue),
            NE_REFLECT_FIELD(wholeNumbers),
            NE_REFLECT_FIELD(direction),
            NE_REFLECT_FIELD(fillRect),
            NE_REFLECT_FIELD(handleRect),
            NE_REFLECT_FIELD(backgroundRect),
            NE_REFLECT_FIELD(interactable)
        NE_REFLECT_END()

        // Helper functions
        float GetNormalizedValue() const {
            if (maxValue <= minValue) return 0.0f;
            return (value - minValue) / (maxValue - minValue);
        }

        void SetNormalizedValue(float normalized) {
            normalized = normalized < 0.0f ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
            value = minValue + normalized * (maxValue - minValue);
            if (wholeNumbers) {
                value = static_cast<float>(static_cast<int>(value + 0.5f));
            }
            ClampValue();
        }

        void ClampValue() {
            if (value < minValue) value = minValue;
            if (value > maxValue) value = maxValue;
        }

        bool IsHorizontal() const {
            return direction == Direction::LEFT_TO_RIGHT || direction == Direction::RIGHT_TO_LEFT;
        }

        bool IsReversed() const {
            return direction == Direction::RIGHT_TO_LEFT || direction == Direction::TOP_TO_BOTTOM;
        }
    };

} // namespace NE::ECS::Component

#endif // UI_SLIDER_HPP
