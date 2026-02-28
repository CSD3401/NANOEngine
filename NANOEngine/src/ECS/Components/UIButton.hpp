#ifndef UI_BUTTON_HPP
#define UI_BUTTON_HPP

#include "../../Math/Vec4.hpp"
#include "Core/Reflection.hpp"
#include <memory>
#include <cstdint>

namespace NE::ECS::Component {

    struct UIButton {
        enum class State { NORMAL, HOVERED, PRESSED, DISABLED };

        // === SERIALIZED FIELDS ===
        // Visual states
        NE::Math::Vec4 normalColor{ 0.8f, 0.8f, 0.8f, 1.0f };
        NE::Math::Vec4 hoverColor{ 0.9f, 0.9f, 0.9f, 1.0f };
        NE::Math::Vec4 pressedColor{ 0.6f, 0.6f, 0.6f, 1.0f };
        NE::Math::Vec4 disabledColor{ 0.5f, 0.5f, 0.5f, 0.5f };

        // Callback/event system
        uint32_t onClickEventId = 0; // For your event system
        bool interactable = true;

        // === RUNTIME FIELDS (not serialized) ===
        State currentState = State::NORMAL;
        bool wasClicked = false;       // Set true on PRESSED->NORMAL transition, cleared each frame
        State previousState = State::NORMAL;  // For state transition detection

        // Reflection - only serialize user-editable fields
        NE_REFLECT_BEGIN(UIButton)
            NE_REFLECT_FIELD(normalColor),
            NE_REFLECT_FIELD(hoverColor),
            NE_REFLECT_FIELD(pressedColor),
            NE_REFLECT_FIELD(disabledColor),
            NE_REFLECT_FIELD(onClickEventId),
            NE_REFLECT_FIELD(interactable)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
#endif // END UI_BUTTON_HPP