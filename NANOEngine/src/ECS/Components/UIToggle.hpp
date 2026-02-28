#ifndef UI_TOGGLE_HPP
#define UI_TOGGLE_HPP

#include "Core/Reflection.hpp"
#include <cstdint>

namespace NE::ECS::Component {

    struct UIToggle {
        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;

        bool isOn = false;

        // Child entity reference - the checkmark graphic to show/hide
        uint32_t graphic = UINT32_MAX;

        // Optional: background entity
        uint32_t background = UINT32_MAX;

        bool interactable = true;

        // Toggle group (0 = no group, same value = same group - only one can be on)
        uint32_t toggleGroup = 0;

        // === RUNTIME-ONLY FIELDS (not serialized) ===
        bool valueChanged = false;  // True for one frame when value changes (for script polling)
        bool previousValue = false; // For detecting changes
        bool wasClicked = false;    // For input handling

        // Reflection
        NE_REFLECT_BEGIN(UIToggle)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(isOn),
            NE_REFLECT_FIELD(graphic),
            NE_REFLECT_FIELD(background),
            NE_REFLECT_FIELD(interactable),
            NE_REFLECT_FIELD(toggleGroup)
        NE_REFLECT_END()

        // Helper to toggle the value
        void Toggle() {
            isOn = !isOn;
            valueChanged = true;
        }

        void SetIsOn(bool value) {
            if (isOn != value) {
                isOn = value;
                valueChanged = true;
            }
        }
    };

} // namespace NE::ECS::Component

#endif // UI_TOGGLE_HPP
