#pragma once

#include <unordered_map>
#include <functional>
#include <typeindex>

namespace Editor {
    class ComponentInspectorRegistry {
    public:
        using InspectorFunc = std::function<void(uint32_t)>;

        static ComponentInspectorRegistry& Get() {
            static ComponentInspectorRegistry instance;
            return instance;
        }

        template<typename Component>
        void Register(InspectorFunc func) {
            m_registry[std::type_index(typeid(Component))] = std::move(func);
        }

        template<typename Component>
        InspectorFunc* Get() {
            auto it = m_registry.find(std::type_index(typeid(Component)));
            if (it != m_registry.end())
                return &it->second;
            return nullptr;
        }

        const std::unordered_map<std::type_index, InspectorFunc>& Inspectors() const {
            return m_registry;
        }

    private:
        std::unordered_map<std::type_index, InspectorFunc> m_registry;
    };
}