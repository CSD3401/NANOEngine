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

        // Child entity references — stored as Hierarchy luids for stable cross-load identity
        uint64_t fillRectLuid = 0;
        uint64_t handleRectLuid = 0;
        uint64_t backgroundRectLuid = 0;
        uint64_t fillAreaRectLuid = 0;
        uint64_t handleSlideAreaRectLuid = 0;

        bool interactable = true;

        // Reflection
        NE_REFLECT_BEGIN(UISlider)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(value),
            NE_REFLECT_FIELD(minValue),
            NE_REFLECT_FIELD(maxValue),
            NE_REFLECT_FIELD(wholeNumbers),
            NE_REFLECT_FIELD(direction),
            NE_REFLECT_FIELD_HIDDEN(fillRectLuid),
            NE_REFLECT_FIELD_HIDDEN(handleRectLuid),
            NE_REFLECT_FIELD_HIDDEN(backgroundRectLuid),
            NE_REFLECT_FIELD_HIDDEN(fillAreaRectLuid),
            NE_REFLECT_FIELD_HIDDEN(handleSlideAreaRectLuid),
            NE_REFLECT_FIELD(interactable)
        NE_REFLECT_END()

        // === RUNTIME-ONLY FIELDS (resolved from luids after load, not serialized) ===
        uint32_t fillRect = UINT32_MAX;
        uint32_t handleRect = UINT32_MAX;
        uint32_t backgroundRect = UINT32_MAX;
        uint32_t fillAreaRect = UINT32_MAX;
        uint32_t handleSlideAreaRect = UINT32_MAX;

        bool isDragging = false;
        bool valueChanged = false;  // True for one frame when value changes (for script polling)
        float previousValue = 0.0f; // For detecting changes

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
