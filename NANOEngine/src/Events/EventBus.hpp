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
        std::shared_ptr<Subscription> Subscribe(EventDomain domain, Listener<EventT> listener) {
            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            std::scoped_lock lock(mutex_);

            auto& base = callbacks_[key];
            if (!base) {
                base = std::make_unique<TypedCallback<EventT>>();
            }

            auto* typed = static_cast<TypedCallback<EventT>*>(base.get());

            // Use shared_ptr for lifetime tracking
            auto listenerPtr = std::make_shared<Listener<EventT>>(std::move(listener));
            typed->listeners.emplace_back(*listenerPtr);

            auto sub = std::make_shared<Subscription>();
            std::weak_ptr<Listener<EventT>> weakListener = listenerPtr;

            sub->unsubscribe = [this, key, weakListener]() {
                std::scoped_lock lock(mutex_);
                auto it = callbacks_.find(key);
                if (it == callbacks_.end()) return;

                auto* typed = static_cast<TypedCallback<EventT>*>(it->second.get());
                // Remove by comparing addresses instead of using indices
                auto listenerLock = weakListener.lock();
                if (listenerLock) {
                    typed->listeners.erase(
                        std::remove_if(typed->listeners.begin(), typed->listeners.end(),
                            [&](const Listener<EventT>& l) {
                                return &l == listenerLock.get();
                            }),
                        typed->listeners.end()
                    );
                }
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
            thread_local bool dispatching = false;
            if (dispatching) return; // ignore recursive dispatch of same event type
            dispatching = true;

            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            // Copy listeners under lock, but call them after unlocking
            std::vector<std::function<void(const EventT&)>> listenersCopy;
            {
                std::scoped_lock lock(mutex_);
                auto it = callbacks_.find(key);
                if (it != callbacks_.end()) {
                    auto* typed = static_cast<TypedCallback<EventT>*>(it->second.get());
                    listenersCopy.reserve(typed->listeners.size());
                    for (const auto& listener : typed->listeners) {
                        if (listener) // Skip null listeners
                            listenersCopy.push_back(listener);
                    }
                }
            }

            // Now call listeners outside of the lock
            for (auto& listener : listenersCopy) {
                listener(event);
            }

            dispatching = false;
        }

        /**
         * Clear all event listeners for a specific domain.
         * CRITICAL: This must be called when stopping play mode to prevent dangling function pointers.
         * @param domain The event domain to clear (e.g., EventDomain::Script)
         */
        void ClearDomain(EventDomain domain) {
            std::scoped_lock lock(mutex_);
            
            // Remove all callbacks that match the domain
            auto it = callbacks_.begin();
            while (it != callbacks_.end()) {
                if (it->first.first == domain) {
                    it = callbacks_.erase(it);
                } else {
                    ++it;
                }
            }
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
    NANOENGINE_API void RegisterScriptEventListener(const char* eventName, std::function<void(void* data)> callback);
}

#pragma warning(pop)
