#include "LightSystem.hpp"
#include "../../Core/Profiler.hpp"

namespace NANOEngine::ECS::Systems {

    LightSystem::LightSystem(ComponentManager* cm)
        : m_componentManager(cm) {}

    void LightSystem::OnEntityAdded(Entity) {}

    void LightSystem::OnEntityRemoved(Entity) {}

    void LightSystem::Init() {}

    void LightSystem::Update(double) {
        NE_PROFILE_FUNCTION();
        //m_directionalLights.clear();
        //for (Entity e : m_componentManager->GetEntitiesWithComponent<Component::DirectionalLight>())
        //    m_directionalLights.push_back(&m_componentManager->GetComponent<Component::DirectionalLight>(e));

        //m_pointLights.clear();
        //for (Entity e : m_componentManager->GetEntitiesWithComponent<Component::PointLight>())
        //    m_pointLights.push_back(&m_componentManager->GetComponent<Component::PointLight>(e));

        //m_spotLights.clear();
        //for (Entity e : m_componentManager->GetEntitiesWithComponent<Component::SpotLight>())
        //    m_spotLights.push_back(&m_componentManager->GetComponent<Component::SpotLight>(e));

        // TODO: Upload light data to GPU
    }

    void LightSystem::Exit() {}

}