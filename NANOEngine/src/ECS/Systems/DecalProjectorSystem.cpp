#include "DecalProjectorSystem.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/DecalProjector.hpp"

#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/DecalCommand.hpp"
#include "Graphics/Core/DecalGizmoCommand.hpp"
#include "Graphics/Core/RenderQueue.hpp"

#include "ResourceManagement/ResourceManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Core/Profiler.hpp"

#include "Math/Mat4.hpp"
#include "Math/Vec4.hpp"
#include <glad/glad.h>
#include <algorithm>
#include <vector>

namespace NE::ECS::Systems {
    namespace {
        inline Math::Vec3 EncodeEntityIdRGB(Entity entity) {
            float r = static_cast<float>(entity & 0xFF) / 255.0f;
            float g = static_cast<float>((entity >> 8) & 0xFF) / 255.0f;
            float b = static_cast<float>((entity >> 16) & 0xFF) / 255.0f;
            return { r, g, b };
        }

        inline Math::Vec3 TransformPoint(const Math::Mat4& M, const Math::Vec3& p) {
            Math::Vec4 v = M * Math::Vec4(p.x, p.y, p.z, 1.0f);
            return { v.x, v.y, v.z };
        }

        void ConfigureDecalMaterial(const std::shared_ptr<Graphics::Material>& material) {
            if (!material || !material->GetPipeline()) return;

            material->SetShader("nedecalprojected");
            material->SetQueueBase(Graphics::RenderQueue::OVERLAY);
            material->SetQueueOffset(0);

            if (material->GetPipeline()) {
                auto spec = material->GetPipeline()->GetSpecification();
                spec.EnableBlending = true;
                spec.EnableDepthTest = false;
                spec.DepthWrite = false;
                spec.CullMode = GL_NONE;
                spec.PolygonMode = GL_FILL;
                material->ApplyPipelineSpec(spec);
            }
        }

#ifndef PRODUCTION_BUILD
        void QueueProjectionBoxGizmo(const Math::Mat4& projectorModel, const Math::Vec3& color) {
            static constexpr Math::Vec3 kCorners[8] = {
                { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f },
                { -0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f }, { 0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }
            };
            static constexpr int kEdges[12][2] = {
                {0,1}, {1,2}, {2,3}, {3,0},
                {4,5}, {5,6}, {6,7}, {7,4},
                {0,4}, {1,5}, {2,6}, {3,7}
            };

            std::vector<Math::Vec3> vertices;
            vertices.reserve(24);

            Math::Vec3 wsCorners[8];
            for (int i = 0; i < 8; ++i) {
                wsCorners[i] = TransformPoint(projectorModel, kCorners[i]);
            }

            for (const auto& edge : kEdges) {
                vertices.push_back(wsCorners[edge[0]]);
                vertices.push_back(wsCorners[edge[1]]);
            }

            Graphics::GraphicsManager::AddDebugLinesBatch(vertices, color);
        }
#endif
    }

    DecalProjectorSystem::DecalProjectorSystem(ComponentManager* cm, Core::LUIDRegistry* lr)
        : m_componentManager(cm), m_luidRegistry(lr) {
    }

    void DecalProjectorSystem::OnEntityAdded(Entity entity) {
        auto& decal = m_componentManager->GetComponent<Component::DecalProjector>(entity);

        std::shared_ptr<Graphics::Material> sourceMaterial;
        if (!decal.materialUUID.empty()) {
            sourceMaterial = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(decal.materialUUID);
        }

        if (!sourceMaterial) {
            sourceMaterial = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
        }

        if (sourceMaterial) {
            decal.material = std::make_shared<Graphics::Material>(*sourceMaterial);
            ConfigureDecalMaterial(decal.material);
        }

        if (decal.luid == 0)
            decal.luid = Core::LUIDGenerator::Generate("dp");

        m_luidRegistry->Register(decal.luid, &decal, entity);
    }

    void DecalProjectorSystem::OnEntityRemoved(Entity entity) {
        if (!m_componentManager->HasComponent<Component::DecalProjector>(entity)) return;
        auto& decal = m_componentManager->GetComponent<Component::DecalProjector>(entity);
        m_luidRegistry->Unregister(decal.luid);
    }

    void DecalProjectorSystem::Init() {
    }

    void DecalProjectorSystem::Update(double /*deltaTime*/) {
#ifndef PRODUCTION_BUILD
        NE_PROFILE_FUNCTION();
#endif
        const auto& entities = m_entities.GetDenseContainer();

        for (Entity entity : entities) {
            const auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(entity);
            if (!meta.isActive) {
                continue;
            }

            auto& decal = m_componentManager->GetComponent<Component::DecalProjector>(entity);
            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
            if (!decal.material || !decal.material->GetPipeline()) continue;

            decal.material->SetUniformFloat("u_DecalOpacity", std::clamp(decal.opacity, 0.0f, 1.0f));
            decal.material->SetUniformVec3("u_DecalTiling", { decal.tilling.x, decal.tilling.y, 0.0f });
            decal.material->SetUniformVec3("u_DecalOffset", { decal.offset.x, decal.offset.y, 0.0f });

            const Math::Mat4 localPivot = Math::Mat4::BuildTranslation(decal.pivot);
            const Math::Mat4 localScale = Math::Mat4::BuildScaling(decal.width, decal.height, decal.depth);
            const Math::Mat4 projectorModel = transform.worldMatrix * localPivot * localScale;

            Graphics::DecalCommand cmd{};
            cmd.model = projectorModel;
            cmd.invModel = projectorModel.Inverse();
            cmd.material = decal.material;
            cmd.positionWS = projectorModel.GetTranslation();
            cmd.drawDistance = std::max(0.0f, decal.drawDistance);
            cmd.startFadeDistance = std::max(0.0f, decal.startFadeDistance);

            Graphics::GraphicsManager::SubmitDecal(cmd);

#ifndef PRODUCTION_BUILD
            Graphics::DecalGizmoCommand gizmoCommand{};
            gizmoCommand.position = cmd.positionWS;
            gizmoCommand.idRGB = EncodeEntityIdRGB(entity);
            Graphics::GraphicsManager::SubmitDecalGizmo(gizmoCommand);

            QueueProjectionBoxGizmo(projectorModel, { 0.20f, 0.95f, 1.0f });
#endif
        }
    }

    void DecalProjectorSystem::Exit() {
    }
}
