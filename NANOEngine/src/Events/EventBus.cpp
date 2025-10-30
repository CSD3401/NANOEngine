#include "EventBus.hpp"

namespace NANOEngine::Events {

    EventBus& EventBus::Get() {
        static EventBus instance;
        return instance;
    }

    struct ScriptEvent {
        std::string name;
        void* data;
    };

    NANOENGINE_API void SendScriptEvent(const char* eventName, void* data)
    {
        ScriptEvent e{ eventName, data };
        EventBus::Get().Dispatch(EventDomain::Script, e);
    }

    NANOENGINE_API void RegisterScriptEventListener(const char* eventName, void(*callback)(void* data))
    {
        EventBus::Get().Subscribe<ScriptEvent>(EventDomain::Script, [=](const ScriptEvent& e) {
            if (e.name == eventName)
                callback(e.data);
            });
    }

}
