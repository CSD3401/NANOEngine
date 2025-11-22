#include "LightSystem.hpp"
#include "../../Core/Profiler.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../ECS/Components/Transform.hpp"
#include "../Components/Light.hpp"

namespace NE::ECS::Systems {

    LightSystem::LightSystem(NE::ECS::ComponentManager* cm)
        : m_componentManager(cm) {}

    void LightSystem::OnEntityAdded(Entity entity) {
        auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
        auto& sl = m_componentManager->GetComponent<Component::Light>(entity);
        sl.position = t.localPosition;
        Graphics::GraphicsManager::m_lights.push_back(&sl);
    }

    void LightSystem::OnEntityRemoved(Entity entity) {
        auto& sl = m_componentManager->GetComponent<Component::Light>(entity);

        Graphics::GraphicsManager::m_lights.erase(
            std::remove_if(Graphics::GraphicsManager::m_lights.begin(), Graphics::GraphicsManager::m_lights.end(),
                [&sl](Component::Light * lightPtr) {
                    return lightPtr == &sl;
                }),
            Graphics::GraphicsManager::m_lights.end()
        );
    }

    void LightSystem::Init() {}

    void LightSystem::Update(double) {
        NE_PROFILE_FUNCTION();            
        // Can optimize with isdirty for light next time
        const auto& entities = GetEntities();
        for (Entity entity : entities) {
            auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
            auto& sl = m_componentManager->GetComponent<Component::Light>(entity);
            sl.position = t.worldMatrix.GetTranslation();
        }
    }

    void LightSystem::Exit() {}

}