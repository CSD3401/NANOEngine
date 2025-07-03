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

        RegisterComponent<Renderer>();
        RegisterComponent<Transform>();

        m_transformSystem = m_systemManager->RegisterSystem<Systems::TransformSystem>(m_componentManager.get());
        SetSystemSignature<Systems::TransformSystem>(
            Signature{}.set(GetComponentType<Transform>())
        );

        m_renderSystem = m_systemManager->RegisterSystem<Systems::RenderSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Transform>());
            sig.set(GetComponentType<Renderer>());
            SetSystemSignature<Systems::RenderSystem>(sig);
        }
    }

    Signature ECSCoordinator::GetSignature(Entity entity)
    {
        return m_entityManager->GetSignature(entity);
    }

}