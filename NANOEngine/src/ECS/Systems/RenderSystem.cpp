#include "RenderSystem.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Light.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/Vertex.hpp"
#include "../../Graphics/OpenGL/GLVertexBuffer.hpp"
#include "../../Graphics/OpenGL/GLIndexBuffer.hpp"
#include "../../Graphics/OpenGL/GLGeometryBuffer.hpp"
#include "../../Graphics/OpenGL/GLShader.hpp"
#include "../../Graphics/OpenGL/GLPipeline.hpp"
#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/DrawCommand.hpp"
#include "../../Core/Profiler.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <glad/glad.h>
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"

using namespace NE::Math;
using NE::Graphics::Frustum;
using NE::Graphics::GraphicsManager;

namespace NE::ECS::Systems {
    namespace {
        inline bool SphereInFrustum(const NE::Graphics::Frustum& f,
            const NE::Math::Vec3& c,
            float r)
        {
            for (int i = 0; i < 6; ++i) {
                const auto& p = f.planes[i];
                float dist = p.n.x * c.x + p.n.y * c.y + p.n.z * c.z + p.d;
                //float dist = p.n.Dot(c) + p.d;
                if (dist < -r) return false;
            }
            return true;
        }

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
        } else {
            renderer.material = Resource::ResourceManager::GetInstance().
                LoadResource<Graphics::Material>("nelitmat");
        }
        if (!renderer.modelUUID.empty())
            renderer.model = Resource::ResourceManager::GetInstance().
            LoadResource<Graphics::Model>(renderer.modelUUID);

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

        const Frustum frustum = BuildFrustum();
        const auto& entities = GetEntities();

        for (Entity entity : entities) {
            // Skip inactive entities
            if (m_componentManager->HasComponent<Component::EntityMeta>(entity)) {
                const auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(entity);
                if (!meta.isActive) {
                    continue;
                }
            }

            auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);
            if (!renderer.model) continue;

            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);


            if (renderer.subMeshIndex < 0) {
                continue;
       //         const auto& ms = renderer.model->localSphere;
       //         float baseR = ms.radius;

       //         bool visible = true;
       //         if (baseR > 0.0f) {
       //             Vec3 centerWS = TransformPoint(transform.worldMatrix, ms.center);
       //             float rWS = baseR * MaxScaleAxis(transform.worldMatrix);
       //             visible = SphereInFrustum(frustum, centerWS, rWS);
       //         }

       //         if (!visible) continue;

			    //for (auto& sub : renderer.model->meshes) {
				   // Graphics::DrawCommand cmd;
				   // cmd.mesh = sub.buffer;
				   // cmd.material = renderer.material;
				   // cmd.transform = transform.worldMatrix;

       //             float r = (float)(entity & 0xFF) / 255.0f;
       //             float g = (float)((entity >> 8) & 0xFF) / 255.0f;
       //             float b = (float)((entity >> 16) & 0xFF) / 255.0f;
				   // cmd.idRGB = Vec3{ r, g, b };

       //             cmd.castsShadow = (renderer.shadowCastMode != Component::Renderer::ShadowCastMode::Off);
       //             cmd.receivesShadow = renderer.receiveShadows;

				   // Graphics::GraphicsManager::Submit(cmd);
			    //}
            } else {
                const auto& ms = renderer.model->meshes[renderer.subMeshIndex].localSphere;
                float baseR = ms.radius;

                bool visible = true;
                if (baseR > 0.0f) {
                    Vec3 centerWS = TransformPoint(transform.worldMatrix, ms.center);
                    float rWS = baseR * MaxScaleAxis(transform.worldMatrix);
                    visible = SphereInFrustum(frustum, centerWS, rWS);
                }

                if (!visible) continue;

                Graphics::DrawCommand cmd;
                cmd.mesh = renderer.model->meshes[renderer.subMeshIndex].buffer;
                cmd.material = renderer.material;
                cmd.transform = transform.worldMatrix;

                float r = (float)(entity & 0xFF) / 255.0f;
                float g = (float)((entity >> 8) & 0xFF) / 255.0f;
                float b = (float)((entity >> 16) & 0xFF) / 255.0f;
                cmd.idRGB = Vec3{ r, g, b };

                cmd.castsShadow = (renderer.shadowCastMode != Component::Renderer::ShadowCastMode::Off);
                cmd.receivesShadow = renderer.receiveShadows;

                Graphics::GraphicsManager::Submit(cmd);
            }
        }
    }

    void RenderSystem::Exit()
    {
    }

    Frustum RenderSystem::BuildFrustum() {
        auto* cam = GraphicsManager::GetEditorCamera();

        if (!cam)
        {
            return Frustum::ExtractPlanesFromVP(Mat4{}); // default
        }

        const Mat4& V = cam->GetViewMatrix();
        const Mat4& P = cam->GetProjectionMatrix();

        Mat4 nonConstPCopy = P;
        return Frustum::ExtractPlanesFromVP(nonConstPCopy * V);
    }

}
