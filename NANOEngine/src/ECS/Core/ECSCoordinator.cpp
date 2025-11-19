#include "ECSCoordinator.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Light.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/NativeScript.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/Camera.hpp"

#include "../Systems/TransformSystem.hpp"
#include "../Systems/RenderSystem.hpp"
#include "../Systems/LightSystem.hpp"
#include "../Systems/RigidbodySystem.hpp"
#include "../Systems/ColliderSystem.hpp"
#include "../Systems/AudioSystem.hpp"
#include "../Systems/ScriptSystem.hpp"
#include "../Systems/UIRenderSystem.hpp"
#include "../Systems/CameraSystem.hpp"
#include "../Systems/PhysicsSystem.hpp"

#include "../Components/Animator.hpp"
#include "../Systems/AnimatorSystem.hpp"  



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
        RegisterComponent<Component::UIRectTransform>();
        RegisterComponent<Component::UICanvas>();
        RegisterComponent<Component::UIImage>();
        RegisterComponent<Component::Animator>();
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

        // I realised only after making PhysicsSystem that this collider system was what is suppose to do - RF
        //m_colliderSystem = m_systemManager->RegisterSystem<Systems::ColliderSystem>(m_componentManager.get());
        //{
        //    Signature sig;
        //    sig.set(GetComponentType<Component::Collider>());
        //    SetSystemSignature<Systems::ColliderSystem>(sig);
        //}

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

        m_uiRenderSystem = m_systemManager->RegisterSystem<Systems::UIRenderSystem>(m_componentManager.get());
        {
            Signature sig;
            //sig.set(m_componentManager->GetComponentType<NE::ECS::Component::UICanvas>());
            sig.set(m_componentManager->GetComponentType<NE::ECS::Component::UIRectTransform>());
            sig.set(m_componentManager->GetComponentType<NE::ECS::Component::UIImage>());
            SetSystemSignature<Systems::UIRenderSystem>(sig);
        }

        m_animatorSystem = m_systemManager->RegisterSystem<Systems::AnimatorSystem>(m_componentManager.get()); // <-- ADD
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());  // Animator works on Transform
            sig.set(GetComponentType<Component::Animator>());   // and requires Animator
            SetSystemSignature<Systems::AnimatorSystem>(sig);
        }
        
        m_cameraSystem = m_systemManager->RegisterSystem<Systems::CameraSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Camera>());
            sig.set(GetComponentType<Component::Transform>());
            SetSystemSignature<Systems::CameraSystem>(sig);
		}
        
        m_physicsSystem = m_systemManager->RegisterSystem<Systems::PhysicsSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Collider>());
            SetSystemSignature<Systems::PhysicsSystem>(sig);
        }
    }

    Entity ECSCoordinator::CreateEntity() {
        Entity entt = m_entityManager->CreateEntity();

        return entt;
    }

    Entity ECSCoordinator::CreateUIEntity()
    {
        Entity entt = m_entityManager->CreateEntity();
        AddComponent(entt, Component::EntityMeta{ "Unnamed UI Entity" });
        AddComponent(entt, Component::UIRectTransform{});
        AddComponent(entt, Component::UIImage{});
        return entt;
    }

    Entity ECSCoordinator::CreateUICanvasEntity()
    {
        Entity entt = m_entityManager->CreateEntity();
        AddComponent(entt, Component::EntityMeta{ "Canvas" });
        AddComponent(entt, Component::UICanvas{});
        Component::UIRectTransform rectTransform;
        rectTransform.x = 0.0f;
        rectTransform.y = 0.0f;
        rectTransform.width = 1920.0f;
        rectTransform.height = 1080.0f;
        rectTransform.parent = NO_ENTITY;  // Canvas has no parent - it's the root

        AddComponent(entt, rectTransform);

        return entt;
    }

    Entity ECSCoordinator::CreateUIImageEntity(Entity parentCanvas)
    {
        Entity entt = m_entityManager->CreateEntity();
        AddComponent(entt, Component::EntityMeta{ "UI Image" });

        // Set up rect transform with parent
        Component::UIRectTransform rect;
        rect.x = 100.0f;
        rect.y = 100.0f;
        rect.width = 100.0f;
        rect.height = 100.0f;
        rect.parent = parentCanvas;  // Link to parent canvas
        AddComponent(entt, rect);

        // Set up image with default color (white = show texture as-is)
        Component::UIImage img;
        img.color = Math::Vec4{ 1.f, 1.f, 1.f, 1.f };
        img.material = nullptr;  // Start with solid color
        AddComponent(entt, img);

        return entt;
    }

    //Entity ECSCoordinator::CreateUIImageEntity(Entity parentCanvas) {
    //    Entity e = CreateEntity();

    //    AddComponent(e, Component::EntityMeta{ "Image" });
    //    AddComponent(e, Component::UIRectTransform{
    //        100.0f, 100.0f, 200.0f, 100.0f // x, y, w, h
    //        });
    //    AddComponent(e, Component::UIImage{
    //        .textureId = LoadSomeTextureSomewhere(),
    //        .color = {1, 1, 1, 1}
    //        });

    //    // Parent it under canvas in your scene graph
    //    m_sceneHierarchy->SetParent(e, parentCanvas);

    //    return e;
    //}

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