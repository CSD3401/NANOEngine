#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include "Signature.hpp"
#include "System.hpp"

namespace NE::ECS {

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
        void SetSystemSignature(Signature signature) {
            std::type_index typeIdx = typeid(T);
            assert(m_systems.find(typeIdx) != m_systems.end() && "System not registered.");
            m_signatures[typeIdx] = signature;
        }

        void EntityDestroyed(Entity entity) {
            for (auto& [_, system] : m_systems) {
                if (system->m_entities.Contains(entity)) {
                    system->m_entities.Remove(entity);
                    system->OnEntityRemoved(entity);
                }
            }
        }

        void EntitySignatureChanged(Entity entity, const Signature& entitySig) {
            for (auto& [type, system] : m_systems) {
                const Signature& sysSig = m_signatures[type];

                bool shouldHave = (entitySig & sysSig) == sysSig;
                bool has = system->m_entities.Contains(entity);

                //std::cout << "Checking Entity " << entity
                //    << ": shouldHave=" << shouldHave
                //    << ", has=" << has << std::endl;

                //std::cout << "Entity Signature: " << entitySig << "\nSystem Signature: " << sysSig << std::endl;
                if (shouldHave != has) {
                    if (shouldHave) {
                        //std::cout << "[SYSTEM INSERT] Entity: " << entity << " added to " << type.name() << std::endl;

                        system->m_entities.Insert(entity);
                        system->OnEntityAdded(entity);
                    } else {
                        system->m_entities.Remove(entity);
                        system->OnEntityRemoved(entity);
                    }
                }
            }
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<System>> m_systems;
        std::unordered_map<std::type_index, Signature> m_signatures;
    };

}
