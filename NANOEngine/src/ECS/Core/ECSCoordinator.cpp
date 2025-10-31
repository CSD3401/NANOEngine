#include "ECSCoordinator.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Light.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/NativeScript.hpp"
#include "../Components/Camera.hpp"

#include "../Systems/TransformSystem.hpp"
#include "../Systems/RenderSystem.hpp"
#include "../Systems/LightSystem.hpp"
#include "../Systems/RigidbodySystem.hpp"
#include "../Systems/ColliderSystem.hpp"
#include "../Systems/AudioSystem.hpp"
#include "../Systems/ScriptSystem.hpp"
#include "../Systems/CameraSystem.hpp"


namespace NE::ECS {

    ECSCoordinator::ECSCoordinator()
    : m_entityManager(std::make_unique<EntityManager>())
    , m_componentManager(std::make_unique<ComponentManager>())
    , m_systemManager(std::make_unique<SystemManager>()) 
    {

        RegisterComponent<Component::EntityMeta>();
        RegisterComponent<Component::Transform>();
        RegisterComponent<Component::Renderer>();
        RegisterComponent<Component::Rigidbody>();
        RegisterComponent<Component::Collider>();
        RegisterComponent<Component::Light>();
        RegisterComponent<Component::AudioSource>();
        RegisterComponent<Component::NativeScript>();
		RegisterComponent<Component::Camera>();
        

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

        m_lightSystem = m_systemManager->RegisterSystem<Systems::LightSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Light>());
            SetSystemSignature<Systems::LightSystem>(sig);
        }

        m_rigidbodySystem = m_systemManager->RegisterSystem<Systems::RigidbodySystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Rigidbody>());
            SetSystemSignature<Systems::RigidbodySystem>(sig);
        }

        m_colliderSystem = m_systemManager->RegisterSystem<Systems::ColliderSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Collider>());
            SetSystemSignature<Systems::ColliderSystem>(sig);
        }

        m_audioSystem = m_systemManager->RegisterSystem<Systems::AudioSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::AudioSource>());
            SetSystemSignature<Systems::AudioSystem>(sig);
        }
        
		m_scriptSystem = m_systemManager->RegisterSystem<Systems::ScriptSystem>(m_componentManager.get());
		{
			Signature sig;
			sig.set(GetComponentType<Component::NativeScript>());
			SetSystemSignature<Systems::ScriptSystem>(sig);
		}

        m_cameraSystem = m_systemManager->RegisterSystem<Systems::CameraSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Camera>());
            sig.set(GetComponentType<Component::Transform>());
            SetSystemSignature<Systems::CameraSystem>(sig);
		}
    }

    Entity ECSCoordinator::CreateEntity() {
        Entity entt = m_entityManager->CreateEntity();
        AddComponent(entt, Component::EntityMeta{"Unnamed Entity"});
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