#include "IScript.hpp" 

NE::ECS::Entity IScript::GetEntity() const {
    return m_entity;
}

IScript::~IScript() = default;