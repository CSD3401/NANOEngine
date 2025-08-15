#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include "ComponentPool.hpp"
#include "Entity.hpp"
#include "Component.hpp"

namespace NE::ECS {

    class ComponentManager {
    public:
        template<typename T>
        void RegisterComponent() {
            std::type_index index(typeid(T));
            assert(m_pools.find(index) == m_pools.end());
            m_pools[index] = std::make_unique<Wrapper<T>>();

            m_componentTypes[index] = m_nextComponentType++;
        }

        template<typename T>
        void AddComponent(Entity e, const T& component) {
            GetPool<T>()->Insert(e, component);
        }

        template<typename T>
        void AddComponent(Entity e, T&& comp) {
            GetPool<T>()->Insert(e, std::forward<T>(comp));
        }

        template<typename T>
        void RemoveComponent(Entity e) {
            GetPool<T>()->Remove(e);
        }

        template<typename T>
        T& GetComponent(Entity e) {
            return GetPool<T>()->Get(e);
        }

        template<typename T>
        bool HasComponent(Entity e) {
            return GetPool<T>()->Has(e);
        }

        template<typename T>
        ComponentType GetComponentType() {
            std::type_index index = typeid(T);
            assert(m_componentTypes.find(index) != m_componentTypes.end() && "Component not registered.");
            return m_componentTypes[index];
        }

        template<typename T>
        const std::vector<Entity>& GetEntitiesWithComponent() const {
            return GetPool<T>()->Entities();
        }

        void EntityDestroyed(Entity e) {
            for (auto const& [_, pool] : m_pools)
                pool->EntityDestroyed(e);
        }

        // For runtime editor usage
        const std::unordered_map<std::type_index, ComponentType>& GetComponentTypeMap() const {
            return m_componentTypes;
        }

    private:
        struct IPool {
            virtual ~IPool() = default;
            virtual void EntityDestroyed(Entity e) = 0;
        };

        template<typename T>
        struct Wrapper : IPool {
            ComponentPool<T, Entity, MAX_ENTITIES> pool;
            void EntityDestroyed(Entity e) override { pool.Remove(e); }
        };

        template<typename T>
        ComponentPool<T, Entity, MAX_ENTITIES>* GetPool() {
            std::type_index index(typeid(T));
            assert(m_pools.find(index) != m_pools.end());
            return &static_cast<Wrapper<T>*>(m_pools[index].get())->pool;
        };

        std::unordered_map<std::type_index, std::unique_ptr<IPool>> m_pools;

        std::unordered_map<std::type_index, ComponentType> m_componentTypes;
        ComponentType m_nextComponentType = 0;
    };

}