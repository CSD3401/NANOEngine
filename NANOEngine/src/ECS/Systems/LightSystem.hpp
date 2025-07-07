#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/DirectionalLight.hpp"
#include "../Components/PointLight.hpp"
#include "../Components/SpotLight.hpp"

namespace NANOEngine::ECS::Systems {

    class LightSystem final : public System {
    public:
        explicit LightSystem(ComponentManager* cm);

        void OnEntityAdded(Entity entity) override;
        void OnEntityRemoved(Entity entity) override;

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;

        const std::vector<Component::DirectionalLight*>& GetDirectionalLights() const { return m_directionalLights; }
        const std::vector<Component::PointLight*>& GetPointLights() const { return m_pointLights; }
        const std::vector<Component::SpotLight*>& GetSpotLights() const { return m_spotLights; }

    private:
        ComponentManager* m_componentManager;

        std::vector<Component::DirectionalLight*> m_directionalLights;
        std::vector<Component::PointLight*> m_pointLights;
        std::vector<Component::SpotLight*> m_spotLights;
    };

}