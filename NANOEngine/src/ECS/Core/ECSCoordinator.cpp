#include "pch.h"
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
#include "../Components/UIText.hpp"
#include "../Components/UIButton.hpp"
#include "../Components/UISlider.hpp"
#include "../Components/UIToggle.hpp"
#include "../Components/Camera.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/PrefabLink.hpp"
#include "../Components/PrefabInstance.hpp"
#include "../Components/CharacterController.hpp"
#include "../Components/DecalProjector.hpp"
#include "../Components/UILayoutGroup.hpp"
#include "../Components/UIGridLayoutGroup.hpp"
#include "../Components/UILayoutElement.hpp"
#include "../Components/UIScrollRect.hpp"
#include "../Components/UIAutoSize.hpp"
#include "../Components/UIInputField.hpp"
#include "../Components/UIDropdown.hpp"

#include "../Systems/TransformSystem.hpp"
#include "../Systems/RenderSystem.hpp"
#include "../Systems/LightSystem.hpp"
#include "../Systems/RigidbodySystem.hpp"
#include "../Systems/ColliderSystem.hpp"
#include "../Systems/AudioSystem.hpp"
#include "../Systems/ScriptSystem.hpp"
#include "../Systems/UIRenderSystem.hpp"
#include "../Systems/CameraSystem.hpp"
#include "../Systems/HierarchySystem.hpp"
#include "../Systems/PrefabSystem.hpp"
#include "../Systems/CharacterControllerSystem.hpp"
#include "../Systems/DecalProjectorSystem.hpp"
#include "../Systems/UIEventSystem.hpp"
#include "../Systems/UILayoutEngine.hpp"
#include "../Systems/UILayoutSystem.hpp"

#include "../Components/Animator.hpp"
#include "../Systems/AnimatorSystem.hpp"  
#include "Core/LUIDGenerator.hpp"



namespace NE::ECS {

    ECSCoordinator::ECSCoordinator()
        : m_entityManager(std::make_unique<EntityManager>())
        , m_componentManager(std::make_unique<ComponentManager>())
        , m_systemManager(std::make_unique<SystemManager>()) 
        , m_luidRegistry(std::make_unique<Core::LUIDRegistry>())
    {

        RegisterComponent<Component::EntityMeta>();
        RegisterComponent<Component::Transform>();
        RegisterComponent<Component::Renderer>();
        RegisterComponent<Component::Collider>();
        RegisterComponent<Component::Rigidbody>();
        RegisterComponent<Component::Light>();
        RegisterComponent<Component::AudioSource>();
        RegisterComponent<Component::UIRectTransform>();
        RegisterComponent<Component::UICanvas>();
        RegisterComponent<Component::UIImage>();
        RegisterComponent<Component::UIText>();
        RegisterComponent<Component::UIButton>();
        RegisterComponent<Component::UISlider>();
        RegisterComponent<Component::UIToggle>();
        RegisterComponent<Component::Animator>();
		RegisterComponent<Component::Camera>();
        RegisterComponent<Component::Hierarchy>();
        RegisterComponent<Component::NativeScript>();
        RegisterComponent<Component::PrefabLink>();
        RegisterComponent<Component::PrefabInstance>();
        RegisterComponent<Component::CharacterController>();
        RegisterComponent<Component::DecalProjector>();
        RegisterComponent<Component::UILayoutGroup>();
        RegisterComponent<Component::UIGridLayoutGroup>();
        RegisterComponent<Component::UILayoutElement>();
        RegisterComponent<Component::UIScrollRect>();
        RegisterComponent<Component::UIAutoSize>();
        RegisterComponent<Component::UIInputField>();
        RegisterComponent<Component::UIDropdown>();

        m_transformSystem = m_systemManager->RegisterSystem<Systems::TransformSystem>(m_componentManager.get(), m_luidRegistry.get());
        SetSystemSignature<Systems::TransformSystem>(
            Signature{}.set(GetComponentType<Component::Transform>())
        );

        m_renderSystem = m_systemManager->RegisterSystem<Systems::RenderSystem>(m_componentManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Renderer>());
            SetSystemSignature<Systems::RenderSystem>(sig);
        }

        m_lightSystem = m_systemManager->RegisterSystem<Systems::LightSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Light>());
            SetSystemSignature<Systems::LightSystem>(sig);
        }

        m_rigidbodySystem = m_systemManager->RegisterSystem<Systems::RigidbodySystem>(m_componentManager.get(), m_entityManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Collider>());
            sig.set(GetComponentType<Component::Rigidbody>());
            SetSystemSignature<Systems::RigidbodySystem>(sig);
        }

        m_colliderSystem = m_systemManager->RegisterSystem<Systems::ColliderSystem>(m_componentManager.get(), m_entityManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Collider>());
            SetSystemSignature<Systems::ColliderSystem>(sig);
        }

        m_audioSystem = m_systemManager->RegisterSystem<Systems::AudioSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::AudioSource>());
            SetSystemSignature<Systems::AudioSystem>(sig);
        }
        
		m_scriptSystem = m_systemManager->RegisterSystem<Systems::ScriptSystem>(m_componentManager.get(), m_entityManager.get(), m_luidRegistry.get());
		{
			Signature sig;
			sig.set(GetComponentType<Component::NativeScript>());
			SetSystemSignature<Systems::ScriptSystem>(sig);
		}

        // Create UILayoutEngine (shared utility, not a System)
        m_uiLayoutEngine = std::make_unique<UILayoutEngine>(m_componentManager.get());

        // UIEventSystem processes input before rendering - handles button states and hit testing
        m_uiEventSystem = m_systemManager->RegisterSystem<Systems::UIEventSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::UIRectTransform>());
            SetSystemSignature<Systems::UIEventSystem>(sig);
        }
        m_uiEventSystem->SetLayoutEngine(m_uiLayoutEngine.get());

        m_uiRenderSystem = m_systemManager->RegisterSystem<Systems::UIRenderSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(m_componentManager->GetComponentType<NE::ECS::Component::UIRectTransform>());
            SetSystemSignature<Systems::UIRenderSystem>(sig);
        }
        m_uiRenderSystem->SetLayoutEngine(m_uiLayoutEngine.get());

        m_uiLayoutSystem = m_systemManager->RegisterSystem<Systems::UILayoutSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::UIRectTransform>());
            SetSystemSignature<Systems::UILayoutSystem>(sig);
        }
        m_uiLayoutSystem->SetLayoutEngine(m_uiLayoutEngine.get());

        m_animatorSystem = m_systemManager->RegisterSystem<Systems::AnimatorSystem>(m_componentManager.get(), m_entityManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Animator>());
            SetSystemSignature<Systems::AnimatorSystem>(sig);
        }
        
        m_cameraSystem = m_systemManager->RegisterSystem<Systems::CameraSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Camera>());
            SetSystemSignature<Systems::CameraSystem>(sig);
		}

        m_hierarchySystem = m_systemManager->RegisterSystem<Systems::HierarchySystem>(m_componentManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Hierarchy>());
            SetSystemSignature<Systems::HierarchySystem>(sig);
        }

        m_prefabSystem = m_systemManager->RegisterSystem<Systems::PrefabSystem>(m_componentManager.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::PrefabInstance>());
            SetSystemSignature<Systems::PrefabSystem>(sig);
        }

        m_characterControllerSystem = m_systemManager->RegisterSystem<Systems::CharacterControllerSystem>(m_componentManager.get(), m_entityManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::Collider>());
            sig.set(GetComponentType<Component::CharacterController>());
            SetSystemSignature<Systems::CharacterControllerSystem>(sig);
        }

        m_decalProjectorSystem = m_systemManager->RegisterSystem<Systems::DecalProjectorSystem>(m_componentManager.get(), m_luidRegistry.get());
        {
            Signature sig;
            sig.set(GetComponentType<Component::Transform>());
            sig.set(GetComponentType<Component::DecalProjector>());
            SetSystemSignature<Systems::DecalProjectorSystem>(sig);
        }
    }

    ECSCoordinator::~ECSCoordinator() = default;

    Entity ECSCoordinator::CreateEntity() {
        return m_entityManager->CreateEntity();
    }

    void ECSCoordinator::DestroyEntity(Entity e) {
        m_entityManager->DestroyEntity(e);
        m_systemManager->EntityDestroyed(e);
        m_componentManager->EntityDestroyed(e);
    }

    Signature ECSCoordinator::GetSignature(Entity entity) {
        return m_entityManager->GetSignature(entity);
    }

}
