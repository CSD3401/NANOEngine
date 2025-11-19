#pragma once
#include "EntityManager.hpp"
#include "ComponentManager.hpp"
#include "SystemManager.hpp"

namespace NE::ECS::Systems {
    class TransformSystem;
    class RenderSystem;
    class LightSystem;
    class RigidbodySystem;
    class ColliderSystem; 
    class AudioSystem;
	class ScriptSystem;
    class UIRenderSystem;
    class AnimatorSystem;
	class CameraSystem;
    class PhysicsSystem;
}

namespace NE::ECS {

    class ECSCoordinator {
    public:
        ECSCoordinator();

        // --- Entity API ---
        Entity CreateEntity();

        void DestroyEntity(Entity e);

        // --- UI API ---
        Entity CreateUIEntity();
        Entity CreateUICanvasEntity();
        Entity CreateUIImageEntity(Entity parentCanvas);

        // --- Component API ---
        template<typename T>
        void RegisterComponent() {
            m_componentManager->RegisterComponent<T>();
        }

        template<typename T>
        void AddComponent(Entity e, const T& comp) {
            if (m_componentManager->HasComponent<T>(e)) return;
            m_componentManager->AddComponent<T>(e, comp);

            auto signature = m_entityManager->GetSignature(e);
            signature.set(GetComponentType<T>(), true);
            m_entityManager->SetSignature(e, signature);

            m_systemManager->EntitySignatureChanged(e, signature);
        }

        template<typename T>
        void RemoveComponent(Entity e) {
            m_componentManager->RemoveComponent<T>(e);

            auto signature = m_entityManager->GetSignature(e);
            signature.set(GetComponentType<T>(), false);
            m_entityManager->SetSignature(e, signature);

            m_systemManager->EntitySignatureChanged(e, signature);
        }

        template<typename T>
        T& GetComponent(Entity e) {
            return m_componentManager->GetComponent<T>(e);
        }

        template<typename T>
        bool HasComponent(Entity e) {
            return m_componentManager->HasComponent<T>(e);
        }

        template<typename T>
        ComponentType GetComponentType() {
            return m_componentManager->GetComponentType<T>();
        }

        // --- System API ---
        template<typename T>
        std::shared_ptr<T> RegisterSystem(ComponentManager* cm) {
            return m_systemManager->RegisterSystem<T>(cm);
        }

        template<typename T>
        void SetSystemSignature(Signature sig) {
            m_systemManager->SetSystemSignature<T>(sig);
        }

        // --- Main loop call ---
        //void UpdateSystems(float deltaTime) {
        //    for (auto& [type, sysPtr] : systemManager->GetAllSystems())
        //        sysPtr->Update(deltaTime);
        //}

        ComponentManager& GetComponentManager() {
            return *m_componentManager;
        }
        std::vector<Entity>& GetUsedEntities() { return m_entityManager->GetUsedEntities(); }

        Signature GetSignature(Entity entity);

        // For editor usage
        const std::unordered_map<std::type_index, ComponentType>& GetRegisteredComponentTypes() const {
            return m_componentManager->GetComponentTypeMap();
        }

        std::shared_ptr<Systems::TransformSystem> m_transformSystem;
        std::shared_ptr<Systems::LightSystem> m_lightSystem;
        std::shared_ptr<Systems::RenderSystem> m_renderSystem;
        std::shared_ptr<Systems::RigidbodySystem> m_rigidbodySystem;
        std::shared_ptr<Systems::ColliderSystem> m_colliderSystem;
        std::shared_ptr<Systems::AudioSystem> m_audioSystem;
        std::shared_ptr<Systems::PhysicsSystem> m_physicsSystem;
		std::shared_ptr<Systems::ScriptSystem> m_scriptSystem;
		std::shared_ptr<Systems::UIRenderSystem> m_uiRenderSystem;
        std::shared_ptr<Systems::AnimatorSystem> m_animatorSystem;
        std::shared_ptr<Systems::CameraSystem> m_cameraSystem;

    private:

        std::unique_ptr<EntityManager> m_entityManager;
        std::unique_ptr<ComponentManager> m_componentManager;
        std::unique_ptr<SystemManager> m_systemManager;

    };

}
