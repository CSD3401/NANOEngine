#include "SystemManager.hpp"

namespace NE::ECS {
    void SystemManager::EntityDestroyed(Entity entity) {
        for (auto& [_, system] : m_systems) {
            if (system->m_entities.Contains(entity)) {
                system->m_entities.Remove(entity);
                system->OnEntityRemoved(entity);
            }
        }
    }

    void SystemManager::EntitySignatureChanged(Entity entity, const Signature& entitySig) {
        for (auto& [type, system] : m_systems) {
            const Signature& sysSig = m_signatures[type];

            bool shouldHave = (entitySig & sysSig) == sysSig;
            bool has = system->m_entities.Contains(entity);

            if (shouldHave != has) {
                if (shouldHave) {
                    system->m_entities.Insert(entity);
                    system->OnEntityAdded(entity);
                } else {
                    system->OnEntityRemoved(entity);
                    system->m_entities.Remove(entity);
                }
            }
        }
    }

    void SystemManager::EntityActiveStatusChanged(Entity entity, const Signature& entitySig, bool active) {
        for (auto& [type, system] : m_systems) {
            const Signature& sysSig = m_signatures[type];

            bool shouldHave = (entitySig & sysSig) == sysSig;

            if (shouldHave) {
                if (active) {
                    system->OnEntityActive(entity);
                } else {
                    system->OnEntityInactive(entity);
                }
            }
        }
    }
}