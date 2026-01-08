#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "../../Math/Vec4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct Button {
        // LUID for serialization
        uint64_t luid = 0;

        enum class State : uint8_t { NORMAL, HOVERED, PRESSED, DISABLED };
        State currentState = State::NORMAL;

        // Visual states - color tint applied to the UIImage
        NE::Math::Vec4 normalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        NE::Math::Vec4 hoverColor{ 0.95f, 0.95f, 0.95f, 1.0f };
        NE::Math::Vec4 pressedColor{ 0.8f, 0.8f, 0.8f, 1.0f };
        NE::Math::Vec4 disabledColor{ 0.5f, 0.5f, 0.5f, 0.5f };

        // Interaction
        bool interactable = true;

        // Callback/event system
        uint32_t onClickEventId = 0; // For event system integration

        // Runtime flag for testing/scripts (cleared each frame)
        bool wasClickedThisFrame = false; // Set to true when button is clicked, scripts can check this

        // Reflection
        NE_REFLECT_BEGIN(Button)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(interactable),
            NE_REFLECT_FIELD(normalColor),
            NE_REFLECT_FIELD(hoverColor),
            NE_REFLECT_FIELD(pressedColor),
            NE_REFLECT_FIELD(disabledColor),
            NE_REFLECT_FIELD(onClickEventId)
            NE_REFLECT_END()

            // Helper function to get current color based on state
            NE::Math::Vec4 GetCurrentColor() const {
            if (!interactable) {
                return disabledColor;
            }
            switch (currentState) {
            case State::NORMAL: return normalColor;
            case State::HOVERED: return hoverColor;
            case State::PRESSED: return pressedColor;
            case State::DISABLED: return disabledColor;
            default: return normalColor;
            }
        }
    };

} // namespace NE::ECS::Component
#endif // END UI_BUTTON_HPP