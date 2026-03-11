#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include "Signature.hpp"
#include "System.hpp"

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE::ECS {
    class ComponentManager;
	class EntityManager;
	class ECSCoordinator;

    class SystemManager {
    public:
        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) == m_systems.end() && "System already registered.");
            auto system = std::make_shared<T>(cm);
            m_systems[typeIdx] = system;
            return system;
        }

        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm, EntityManager* em) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) == m_systems.end() && "System already registered.");
            auto system = std::make_shared<T>(cm, em);
            m_systems[typeIdx] = system;
            return system;
        }

        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm, Core::LUIDRegistry* lr) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) == m_systems.end() && "System already registered.");
            auto system = std::make_shared<T>(cm, lr);
            m_systems[typeIdx] = system;
            return system;
        }

        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) == m_systems.end() && "System already registered.");
            auto system = std::make_shared<T>(cm, em, lr);
            m_systems[typeIdx] = system;
            return system;
        }

        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm, ECSCoordinator* ec, Core::LUIDRegistry* lr) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) == m_systems.end() && "System already registered.");
            auto system = std::make_shared<T>(cm, ec, lr);
            m_systems[typeIdx] = system;
            return system;
        }

        template<typename T>
        void SetSystemSignature(Signature signature) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) != m_systems.end() && "System not registered.");
            m_signatures[typeIdx] = signature;
        }

        void EntityDestroyed(Entity entity);
        void EntitySignatureChanged(Entity entity, const Signature& entitySig);
		void EntityActiveStatusChanged(Entity entity, const Signature& entitySig, bool active);

    private:
        std::unordered_map<std::type_index, std::shared_ptr<System>> m_systems;
        std::unordered_map<std::type_index, Signature> m_signatures;
    };

}
