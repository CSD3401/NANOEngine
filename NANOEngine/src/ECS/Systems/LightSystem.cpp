#include "LightSystem.hpp"
#include "../../Core/Profiler.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../ECS/Components/Transform.hpp"
#include "../../ECS/Components/EntityMeta.hpp"
#include "../Components/Light.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::ECS::Systems {

    LightSystem::LightSystem(NE::ECS::ComponentManager* cm)
        : m_componentManager(cm) {}

    void LightSystem::OnEntityAdded(Entity entity) {
        //auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
        //auto& sl = m_componentManager->GetComponent<Component::Light>(entity);
        //sl.position = t.localPosition;
        //Graphics::GraphicsManager::m_lights.push_back(&sl);
    }

    void LightSystem::OnEntityRemoved(Entity entity) {
        //auto& sl = m_componentManager->GetComponent<Component::Light>(entity);

        //Graphics::GraphicsManager::m_lights.erase(
        //    std::remove_if(Graphics::GraphicsManager::m_lights.begin(), Graphics::GraphicsManager::m_lights.end(),
        //        [&sl](Component::Light * lightPtr) {
        //            return lightPtr == &sl;
        //        }),
        //    Graphics::GraphicsManager::m_lights.end()
        //);
    }

    void LightSystem::Init() {
        Graphics::GraphicsManager::m_lights.reserve(12);
    }

    void LightSystem::Update(double) {
        NE_PROFILE_FUNCTION();
        Graphics::GraphicsManager::m_lights.clear();
        int numLights = 0;
        // Can optimize with isdirty for light next time
        const auto& entities = GetEntities();
        for (Entity entity : entities) {
            if (numLights > 12) break;
            auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(entity);
            if (meta.isActive) {
                auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
                auto& sl = m_componentManager->GetComponent<Component::Light>(entity);
                sl.position = t.worldMatrix.GetTranslation();
                Graphics::GraphicsManager::m_lights.push_back(&sl);
                ++numLights;
            }
        }
    }

    void LightSystem::Exit() {
    }

}