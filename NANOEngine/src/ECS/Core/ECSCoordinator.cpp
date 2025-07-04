#include "ECSCoordinator.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Renderer.hpp"

#include "../Systems/TransformSystem.hpp"
#include "../Systems/RenderSystem.hpp"


namespace NANOEngine::ECS {

    ECSCoordinator::ECSCoordinator()
    : m_entityManager(std::make_unique<EntityManager>())
    , m_componentManager(std::make_unique<ComponentManager>())
    , m_systemManager(std::make_unique<SystemManager>()) 
    {

        RegisterComponent<Component::Renderer>();
        RegisterComponent<Component::Transform>();

        m_transformSystem = m_systemManager->RegisterSystem<Systems::TransformSystem>(m_componentManager.get());
        SetSystemSignature<Systems::TransformSystem>(
            Signature{}.set(GetComponentType<Component::Transform>())
        );

        m_renderSystem = m_systemManager->RegisterSystem<Systems::RenderSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Renderer>());
            SetSystemSignature<Systems::RenderSystem>(sig);
        }
    }

    Entity ECSCoordinator::CreateEntity() {
        Entity entt = m_entityManager->CreateEntity();
        AddComponent(entt, Component::Transform{});
        return entt;
    }

    void ECSCoordinator::DestroyEntity(Entity e) {
        m_entityManager->DestroyEntity(e);
        m_componentManager->EntityDestroyed(e);
        m_systemManager->EntityDestroyed(e);
    }

    Signature ECSCoordinator::GetSignature(Entity entity)
    {
        return m_entityManager->GetSignature(entity);
    }

}