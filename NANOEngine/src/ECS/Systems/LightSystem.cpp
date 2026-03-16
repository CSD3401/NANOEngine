#include "pch.h"
#include "LightSystem.hpp"

#include "ECS/Core/ComponentManager.hpp"
#include "ECS/Core/EntityManager.hpp"
#include "Core/Profiler.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/LightGizmoCommand.hpp"
#include "ECS/Components/Transform.hpp"
#include "../Components/Light.hpp"

namespace NE::ECS::Systems {
    namespace {
        inline NE::Math::Vec3 EncodeEntityIdRGB(Entity entity) {
            float r = static_cast<float>(entity & 0xFF) / 255.0f;
            float g = static_cast<float>((entity >> 8) & 0xFF) / 255.0f;
            float b = static_cast<float>((entity >> 16) & 0xFF) / 255.0f;
            return { r, g, b };
        }

        inline NE::Math::Vec3 ComputeLightDirection(
            const Component::Light& light,
            const NE::Math::Vec3& right,
            const NE::Math::Vec3& up,
            const NE::Math::Vec3& forward) {
            if (light.type == Component::Light::Type::Area) {
                auto emitDirection = -right.Cross(up);
                if (emitDirection.LengthSquared() > 1e-6f) {
                    emitDirection.Normalize();
                    return emitDirection;
                }
                return { 0.0f, 0.0f, -1.0f };
            }

            auto lightDirection = forward;
            if (lightDirection.LengthSquared() > 1e-6f) {
                lightDirection.Normalize();
                return lightDirection;
            }
            return { 0.0f, -1.0f, 0.0f };
        }
    }

    LightSystem::LightSystem(ComponentManager* cm, EntityManager* em)
        : m_componentManager(cm), m_entityManager(em) {}

    void LightSystem::OnEntityAdded(Entity /*entity*/) {
    }

    void LightSystem::OnEntityRemoved(Entity /*entity*/) {
    }

    void LightSystem::OnEntityActive(Entity /*entity*/) {}
    void LightSystem::OnEntityInactive(Entity /*entity*/) {}

    void LightSystem::Init() {
        Graphics::GraphicsManager::m_lights.reserve(12);
    }

    void LightSystem::Update(double) {
#ifndef PRODUCTION_BUILD
        NE_PROFILE_FUNCTION();
#endif
        Graphics::GraphicsManager::m_lights.clear();

        const auto& entities = m_entities.GetDenseContainer();
        for (Entity entity : entities) {
            if (m_entityManager->GetActive(entity)) {
                auto& t = m_componentManager->GetComponent<Component::Transform>(entity);
                auto& light = m_componentManager->GetComponent<Component::Light>(entity);
                light.position = t.worldMatrix.GetTranslation();
                light.right = t.worldMatrix.Right();
                light.up = t.worldMatrix.Up();
                light.direction = ComputeLightDirection(light, light.right, light.up, t.worldMatrix.Forward());
                Graphics::GraphicsManager::m_lights.push_back({ entity, &light });

#ifndef PRODUCTION_BUILD
                Graphics::LightGizmoCommand gizmoCommand{};
                gizmoCommand.position = light.position;
                gizmoCommand.idRGB = EncodeEntityIdRGB(entity);
                gizmoCommand.lightType = light.type;
                Graphics::GraphicsManager::SubmitLightGizmo(gizmoCommand);
#endif // !PRODUCTION_BUILD
            }
        }
    }

    void LightSystem::Exit() {
    }
}
