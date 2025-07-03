#include "EventBus.hpp"

namespace NANOEngine::Events {

    EventBus& EventBus::Get() {
        static EventBus instance;
        return instance;
    }

}
