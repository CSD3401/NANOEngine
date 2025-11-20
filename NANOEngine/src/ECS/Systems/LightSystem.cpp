#include "LightSystem.hpp"
#include "../../Core/Profiler.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../ECS/Components/Transform.hpp"
#include "../Components/Light.hpp"

namespace NE::ECS::Systems {

    LightSystem::LightSystem(NE::ECS::ComponentManager* cm)
        : m_componentManager(cm) {}

    void LightSystem::OnEntityAdded(Entity) {}

    void LightSystem::OnEntityRemoved(Entity) {}

    void LightSystem::Init() {}

    void LightSystem::Update(double) {
        NE_PROFILE_FUNCTION();            
        
        Graphics::GraphicsManager::m_lights.clear();

        const auto& entities = GetEntities();
        for (Entity entity : entities) {
            auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
            auto& sl = m_componentManager->GetComponent<Component::Light>(entity);
            sl.position = t.localPosition;
            Graphics::GraphicsManager::m_lights.push_back(&sl);
        }
    }

    void LightSystem::Exit() {}

}