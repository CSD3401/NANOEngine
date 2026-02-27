#include "pch.h"
#include "EventBus.hpp"

namespace NANOEngine::Events {

    EventBus& EventBus::Get() {
        static EventBus instance;
        return instance;
    }

    void EventBus::DispatchQueued() {
        std::queue<std::shared_ptr<IQueuedEvent>> toProcess;
        {
            std::scoped_lock lock(mutex_);
            std::swap(toProcess, queuedEvents_);
        }

        while (!toProcess.empty()) {
            auto e = toProcess.front();
            toProcess.pop();
            e->Dispatch(*this);
        }
    }

    void EventBus::ClearDomain(EventDomain domain)
    {
        std::scoped_lock lock(mutex_);

        auto it = callbacks_.begin();
        while (it != callbacks_.end()) {
            if (it->first.first == domain) {
                it = callbacks_.erase(it);
            }
            else {
                ++it;
            }
        }

        // Also clear queued events from this domain
        std::queue<std::shared_ptr<IQueuedEvent>> filteredQueue;
        while (!queuedEvents_.empty()) {
            auto event = queuedEvents_.front();
            queuedEvents_.pop();
            // need to add domain tracking to IQueuedEvent to filter properly
            // For now, just clear all queued events when clearing script domain
        }
        if (domain == EventDomain::Script) {
            // Clear all queued events when clearing script domain
            queuedEvents_ = std::queue<std::shared_ptr<IQueuedEvent>>();
        }
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

    NANOENGINE_API void RegisterScriptEventListener(
        const char* eventName,
        std::function<void(void* data)> callback)
    {
        using namespace NANOEngine::Events;
        std::string eventNameCopy = eventName;  // Make a copy to avoid dangling pointer
        EventBus::Get().Subscribe<ScriptEvent>(EventDomain::Script, [eventNameCopy, callback](const ScriptEvent& e) {
            if (e.name == eventNameCopy) {
                callback(e.data);
            }
            });
    }

    NANOENGINE_API void ClearScriptEventListeners() {
        EventBus::Get().ClearDomain(EventDomain::Script);
    }

}
