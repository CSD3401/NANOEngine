#pragma once
#include <cstdint>

namespace NANOEngine::Events {

    // UIButton click event - dispatched when a button is clicked
    // Subscribe to this event to handle button clicks
    //
    // Usage Example (C++):
    //   #include "Events/UIButtonEvents.hpp"
    //   #include "Events/EventBus.hpp"
    //
    //   // Subscribe to button clicks
    //   NANOEngine::Events::EventBus::Get().Subscribe<NANOEngine::Events::UIButtonClickEvent>(
    //       NANOEngine::Events::EventDomain::Engine,
    //       [](const NANOEngine::Events::UIButtonClickEvent& event) {
    //           std::cout << "Button " << event.entity << " clicked! Event ID: " << event.eventId << std::endl;
    //           // Handle button click here
    //       }
    //   );
    //
    // Usage Example (Scripts):
    //   // Scripts can subscribe to EventDomain::Script to receive button click events
    //   // The event will contain the entity ID and eventId
    struct UIButtonClickEvent {
        uint32_t entity;      // The entity ID of the button that was clicked
        uint32_t eventId;     // The onClickEventId from the UIButton component (0 if not set)
    };

} // namespace NANOEngine::Events

