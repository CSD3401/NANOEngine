#include "IScript.hpp" 

NE::ECS::Entity IScript::GetEntity() const {
    return m_entity;
}

IScript::~IScript() = default;

void IScript::LinkToEngine(NE::ECS::ComponentManager* componentManager) {
    m_componentManager = componentManager;
}
