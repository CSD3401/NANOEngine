#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include "../../NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

namespace NANOEngine::Events {

    enum class EventDomain { Engine, Editor, Script };

    class NANOENGINE_API EventBus {
    public:
        static EventBus& Get();

        template<class EventT>
        using Listener = std::function<void(const EventT&)>;

        template<class EventT>
        void Subscribe(EventDomain domain, Listener<EventT> listener) {
            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            auto& base = callbacks_[key];
            if (!base) {
                base = std::make_unique<TypedCallback<EventT>>();
            }

            auto* typed = static_cast<TypedCallback<EventT>*>(base.get());
            typed->listeners.emplace_back(std::move(listener));
        }

        template<class EventT>
        void Dispatch(EventDomain domain, const EventT& event) const {
            DomainKey key = { domain, std::type_index(typeid(EventT)) };

            auto it = callbacks_.find(key);
            if (it == callbacks_.end()) return;

            auto* typed = static_cast<TypedCallback<EventT>*>(it->second.get());
            for (const auto& listener : typed->listeners) {
                listener(event);
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

        struct DomainKeyHash {
            std::size_t operator()(const std::pair<EventDomain, std::type_index>& key) const {
                std::size_t h1 = std::hash<int>()(static_cast<int>(key.first));
                std::size_t h2 = std::hash<std::type_index>()(key.second);
                return h1 ^ (h2 << 1);
            }
        };

        using DomainKey = std::pair<EventDomain, std::type_index>;
        mutable std::unordered_map<DomainKey, std::unique_ptr<CallbackBase>, DomainKeyHash> callbacks_;
    };
}

#pragma warning(pop)