#pragma once
#include <cstdint>

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

} // namespace NANOEngine::Events
