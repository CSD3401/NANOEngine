#include "pch.h"
#include "RenderSystem.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "Graphics/Core/GraphicsManager.hpp"

#include "Graphics/Core/Material.hpp"
#include "Graphics/Core/DrawCommand.hpp"
#include "Core/Profiler.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"

namespace NE::ECS::Systems {
    namespace {
        inline float MaxScaleAxis(const NE::Math::Mat4& M) {
            NE::Math::Vec3 x = { M.GetElement(0, 0), M.GetElement(1, 0), M.GetElement(2, 0) };
            NE::Math::Vec3 y = { M.GetElement(0, 1), M.GetElement(1, 1), M.GetElement(2, 1) };
            NE::Math::Vec3 z = { M.GetElement(0, 2), M.GetElement(1, 2), M.GetElement(2, 2) };

            float sx = x.Length();
            float sy = y.Length();
            float sz = z.Length();
            return std::max(sx, std::max(sy, sz));
        }

        inline NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& M, const NE::Math::Vec3& p) {
            NE::Math::Vec4 v = M * NE::Math::Vec4(p.x, p.y, p.z, 1.0f);
            return { v.x, v.y, v.z };
        }
    }

    RenderSystem::RenderSystem(ComponentManager* cm, Core::LUIDRegistry* lr) 
        : m_componentManager(cm), m_luidRegistry(lr)
    {
    }

    void RenderSystem::OnEntityAdded(Entity entity) {
        auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);

        if (!renderer.materialUUID.empty()) {
            renderer.material = Resource::ResourceManager::GetInstance().
                LoadResource<Graphics::Material>(renderer.materialUUID);
            if (!renderer.material) {
				renderer.materialUUID = "Lit";
                renderer.material = Resource::ResourceManager::GetInstance().
                    LoadResource<Graphics::Material>("nelitmat");
            }
        } else {
            renderer.materialUUID = "Lit";
            renderer.material = Resource::ResourceManager::GetInstance().
                LoadResource<Graphics::Material>("nelitmat");
        }

        if (!renderer.modelUUID.empty()) {
            renderer.model = Resource::ResourceManager::GetInstance().
                LoadResource<Graphics::Model>(renderer.modelUUID);
            if (!renderer.model) {
                renderer.modelUUID = "Cube";
                renderer.model = Resource::ResourceManager::GetInstance().
                    LoadResource<Graphics::Model>("builtin:model/cube");
				renderer.subMeshIndex = 0;
            }
        } else {
            renderer.modelUUID = "Cube";
            renderer.material = Resource::ResourceManager::GetInstance().
                LoadResource<Graphics::Material>("builtin:model/cube");
            renderer.subMeshIndex = 0;
        }

        if (renderer.luid == 0)
            renderer.luid = Core::LUIDGenerator::Generate("rd");

        m_luidRegistry->Register(renderer.luid, &renderer, entity);
    }

    void RenderSystem::OnEntityRemoved(Entity e) {
        auto& renderer = m_componentManager->GetComponent<Component::Renderer>(e);
        m_luidRegistry->Unregister(renderer.luid);
    }

    void RenderSystem::Init() {
    }

    void RenderSystem::Update(double /*deltaTime*/) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif
        const auto& entities = m_entities.GetDenseContainer();

        for (Entity entity : entities) {
            const auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(entity);
            if (!meta.isActive) {
                continue;
            }

            auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);
            if (!renderer.model) continue;

            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

            const auto& ms = renderer.model->meshes[renderer.subMeshIndex].localSphere;
            float baseR = ms.radius;

            Graphics::DrawCommand cmd;
            cmd.mesh = renderer.model->meshes[renderer.subMeshIndex].buffer;
            cmd.material = renderer.material;
            cmd.transform = transform.worldMatrix;

            float r = (float)(entity & 0xFF) / 255.0f;
            float g = (float)((entity >> 8) & 0xFF) / 255.0f;
            float b = (float)((entity >> 16) & 0xFF) / 255.0f;
            cmd.idRGB = Math::Vec3{ r, g, b };

            cmd.boundsCenterWS = TransformPoint(transform.worldMatrix, ms.center);
            cmd.boundsRadiusWs = baseR * MaxScaleAxis(transform.worldMatrix);

            cmd.castsShadow = (renderer.shadowCastMode != Component::Renderer::ShadowCastMode::Off);
            cmd.receivesShadow = renderer.receiveShadows;

            Graphics::GraphicsManager::Submit(cmd);
        }
    }

    void RenderSystem::Exit()
    {
    }
}
