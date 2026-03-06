#pragma once

#include "../Core/System.hpp"

namespace NE::ECS {
    class ComponentManager;
    class EntityManager;
}

namespace NE::ECS::Systems {

    class LightSystem final : public System {
    public:
        explicit LightSystem(ComponentManager* cm, EntityManager* em);

        void OnEntityAdded(Entity entity) override;
        void OnEntityRemoved(Entity entity) override;


        void OnEntityActive(Entity entity) override;
        void OnEntityInactive(Entity entity) override;
        void Init() override;
        void Update(double deltaTime) override;
        void UploadLights();
        void Exit() override;

        //const std::vector<Component::DirectionalLight*>& GetDirectionalLights() const { return m_directionalLights; }
        //const std::vector<Component::PointLight*>& GetPointLights() const { return m_pointLights; }
        //const std::vector<Component::SpotLight*>& GetSpotLights() const { return m_spotLights; }

    private:
        ComponentManager* m_componentManager;
		EntityManager* m_entityManager;
        //std::vector<Component::DirectionalLight*> m_directionalLights;
        //std::vector<Component::PointLight*> m_pointLights;
        //std::vector<Component::SpotLight*> m_spotLights;
    };

}