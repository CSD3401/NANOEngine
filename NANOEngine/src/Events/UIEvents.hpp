#pragma once
#include <cstdint>
#include <string>

namespace NANOEngine::Events {

    struct UIButtonClickEvent {
        uint32_t entity;
        uint32_t onClickEventId;
    };

    struct UIToggleChangedEvent {
        uint32_t entity;
        bool isOn;
    };

    struct UISliderValueChangedEvent {
        uint32_t entity;
        float value;
        float previousValue;
    };

    struct UIPointerEnterEvent {
        uint32_t entity;
    };

    struct UIPointerExitEvent {
        uint32_t entity;
    };

    // Focus events
    struct UIFocusEvent {
        uint32_t entity;
    };

    struct UIBlurEvent {
        uint32_t entity;
    };

    // Input field events
    struct UIInputFieldChangedEvent {
        uint32_t entity;
        std::string text;
        std::string previousText;
        uint32_t onValueChangedEventId;
    };

    struct UIInputFieldSubmitEvent {
        uint32_t entity;
        std::string text;
        uint32_t onSubmitEventId;
    };

    // Dropdown events
    struct UIDropdownValueChangedEvent {
        uint32_t entity;
        int selectedIndex;
        int previousIndex;
        std::string selectedOption;
        uint32_t onValueChangedEventId;
    };

    // Drag events
    struct UIPointerDragEvent {
        uint32_t entity;
        float deltaX;
        float deltaY;
        float posX;
        float posY;
    };

} // namespace NANOEngine::Events
