#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <mutex>
#include <queue>
#include "../NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace NANOEngine::Events {

    enum class EventDomain { Engine, Editor, Script };

    class NANOENGINE_API EventBus {

    private:
        struct Subscription {
            std::function<void()> unsubscribe;
            ~Subscription() { if (unsubscribe) unsubscribe(); }
        };

    public:
        static EventBus& Get();

        template<class EventT>
        using Listener = std::function<void(const EventT&)>;

        template<class EventT>
        Subscription Subscribe(EventDomain domain, Listener<EventT> listener) {
            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            auto& base = callbacks_[key];
            if (!base) {
                base = std::make_unique<TypedCallback<EventT>>();
            }

            auto* typed = static_cast<TypedCallback<EventT>*>(base.get());
            typed->listeners.emplace_back(std::move(listener));
            size_t index = typed->listeners.size() - 1;

            // Return an RAII unsubscriber
            Subscription sub;
            sub.unsubscribe = [this, key, index]() {
                auto it = callbacks_.find(key);
                if (it == callbacks_.end()) return;
                auto* typed = static_cast<TypedCallback<EventT>*>(it->second.get());
                if (index < typed->listeners.size())
                    typed->listeners[index] = nullptr; // or erase later for safety
                };
            return sub;
        }


        // New: Queued (deferred) dispatch
        template<class EventT>
        void Queue(EventDomain domain, const EventT& event) {
            std::scoped_lock lock(mutex_);
            queuedEvents_.emplace(std::make_shared<QueuedEvent<EventT>>(domain, event));
        }

        // Called once per frame (main thread)
        void DispatchQueued() {
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

        template<class EventT>
        void Dispatch(EventDomain domain, const EventT& event) const {
            std::scoped_lock lock(mutex_);
            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            auto it = callbacks_.find(key);
            if (it == callbacks_.end()) return;

            auto* typed = static_cast<TypedCallback<EventT>*>(it->second.get());
            for (const auto& listener : typed->listeners)
                listener(event);
        }

        
    private:
        EventBus() = default;
        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;

        struct CallbackBase { virtual ~CallbackBase() = default; };
        template<class EventT>
        struct TypedCallback : CallbackBase {
            std::vector<Listener<EventT>> listeners;
        };

        // For queued events
        struct IQueuedEvent {
            virtual ~IQueuedEvent() = default;
            virtual void Dispatch(EventBus& bus) = 0;
        };

        template<class EventT>
        struct QueuedEvent : IQueuedEvent {
            EventDomain domain;
            EventT event;
            QueuedEvent(EventDomain d, const EventT& e) : domain(d), event(e) {}
            void Dispatch(EventBus& bus) override {
                bus.Dispatch(domain, event);
            }
        };

        struct DomainKeyHash {
            std::size_t operator()(const std::pair<EventDomain, std::type_index>& key) const {
                std::size_t h1 = std::hash<int>()(static_cast<int>(key.first));
                std::size_t h2 = std::hash<std::type_index>()(key.second);
                return h1 ^ (h2 << 1);
            }
        };

        using DomainKey = std::pair<EventDomain, std::type_index>;
        mutable std::mutex mutex_;
        std::unordered_map<DomainKey, std::unique_ptr<CallbackBase>, DomainKeyHash> callbacks_;
        std::queue<std::shared_ptr<IQueuedEvent>> queuedEvents_;
    };

    // Send a generic event to the engine
    NANOENGINE_API void SendScriptEvent(const char* eventName, void* data);

    // Register a script-side callback
    NANOENGINE_API void RegisterScriptEventListener(const char* eventName, void(*callback)(void* data));
}

#pragma warning(pop)
