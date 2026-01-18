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
				renderer.materialUUID = "neunlitmat";
                renderer.material = Resource::ResourceManager::GetInstance().
                    LoadResource<Graphics::Material>(renderer.materialUUID);
			}
        }
        if (!renderer.modelUUID.empty()) {
            renderer.model = Resource::ResourceManager::GetInstance().
            LoadResource<Graphics::Model>(renderer.modelUUID);
            if (!renderer.model) {
                renderer.modelUUID = "builtin:model/cube";
                renderer.model = Resource::ResourceManager::GetInstance().
                    LoadResource<Graphics::Model>(renderer.modelUUID);
            }
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

    void RenderSystem::Update(double deltaTime) {
		NE_PROFILE_FUNCTION();

        FrustumCulling();

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
            if (!renderer.visible || !renderer.model) continue;
            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

			for (auto& sub : renderer.model->meshes) {
				Graphics::DrawCommand cmd;
				cmd.mesh = sub.buffer;
				cmd.material = renderer.material;
				cmd.transform = transform.worldMatrix;

                float r = (float)(entity & 0xFF) / 255.0f;
                float g = (float)((entity >> 8) & 0xFF) / 255.0f;
                float b = (float)((entity >> 16) & 0xFF) / 255.0f;
				cmd.idRGB = Vec3{ r, g, b };

                cmd.castsShadow = (renderer.shadowCastMode != Component::Renderer::ShadowCastMode::Off);
                cmd.receivesShadow = renderer.receiveShadows;

                //cmd.material->SetUniformVec3("u_Material.ambient", { 0.1f, 0.1f, 0.1f });
                //cmd.material->SetUniformVec3("u_Material.diffuse", { 1.0f, 0.5f, 0.31f });
                //cmd.material->SetUniformVec3("u_Material.specular", { 0.5f, 0.5f, 0.5f });
                //cmd.material->SetUniformFloat("u_Material.shininess", 32.0f);
                //if (renderer.model && renderer.model->HasSkeleton()) {
                //    // advance time (dt variable is available in Update)
                //    renderer.model->UpdateAnimation(deltaTime);

                //    // upload bones to the material (Material::Bind will push them to the shader)
                //    const auto& bones = renderer.model->GetBoneMatrices();
                //    if (!bones.empty()) {
                //        renderer.material->SetUniformMat4Array("u_Bones", bones);
                //    }
                //}

				Graphics::GraphicsManager::Submit(cmd);
			}
        }
    }

    void RenderSystem::Exit()
    {
    }

    Frustum RenderSystem::BuildFrustum() 
    {
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

    bool RenderSystem::TestSphereFrustum(const Frustum& F, const Mat4& M, const Vec3& centerLS, float radiusLS) 
    {
        // transform center from local space to world space
        Vec3 centerWS{
            M.a[0] * centerLS.x + M.a[4] * centerLS.y + M.a[8] * centerLS.z + M.a[12],
            M.a[1] * centerLS.x + M.a[5] * centerLS.y + M.a[9] * centerLS.z + M.a[13],
            M.a[2] * centerLS.x + M.a[6] * centerLS.y + M.a[10] * centerLS.z + M.a[14]
        };

        // get the scaling factors
        Vec3 scale = M.GetScale();

        // compute the radius in world space using the largest scaling factor
        float radiusWS = radiusLS * std::max({ scale.x, scale.y, scale.z });

        // test intersection of bounding sphere with frustum
        return F.IntersectsSphere(centerWS, radiusWS);
    }


    //bool RenderSystem::TestAABBFrustum(const Frustum& F, const Mat4& M, const NE::Math::Vec3& minLS, const NE::Math::Vec3& maxLS) {
    //    // 8 local corners of the AABB
    //    const Vec3 cornersLS[8]{
    //        {minLS.x, minLS.y, minLS.z},
    //        {maxLS.x, minLS.y, minLS.z},
    //        {minLS.x, maxLS.y, minLS.z},
    //        {maxLS.x, maxLS.y, minLS.z},
    //        {minLS.x, minLS.y, maxLS.z},
    //        {maxLS.x, minLS.y, maxLS.z},
    //        {minLS.x, maxLS.y, maxLS.z},
    //        {maxLS.x, maxLS.y, maxLS.z},
    //    };

    //    // helps to transform a point in local space to world space
    //    auto pointLSToWS = [](const Mat4& matrix, const Vec3& p) -> Vec3 {
    //        return Vec3{
    //            matrix.a[0] * p.x + matrix.a[4] * p.y + matrix.a[8] * p.z + matrix.a[12],
    //            matrix.a[1] * p.x + matrix.a[5] * p.y + matrix.a[9] * p.z + matrix.a[13],
    //            matrix.a[2] * p.x + matrix.a[6] * p.y + matrix.a[10] * p.z + matrix.a[14]
    //        };
    //        };

    //    // seed with first corner
    //    Vec3 minWS = pointLSToWS(M, cornersLS[0]);
    //    Vec3 maxWS = minWS;

    //    // continue with the remaining corners
    //    for (int i = 1; i < 8; ++i)
    //    {
    //        Vec3 cornerWS = pointLSToWS(M, cornersLS[i]);
    //        minWS = Vec3{ std::min(minWS.x, cornerWS.x), std::min(minWS.y, cornerWS.y), std::min(minWS.z, cornerWS.z) };
    //        maxWS = Vec3{ std::max(maxWS.x, cornerWS.x), std::max(maxWS.y, cornerWS.y), std::max(maxWS.z, cornerWS.z) };
    //    }

    //    // test intersection of AABB with frustum
    //    return F.IntersectsAABB(minWS, maxWS);
    //}

    void RenderSystem::FrustumCulling() {
        //// build frustum from the active camera
        //const Frustum frustum = BuildFrustum();

        //const auto& entities = GetEntities();
        //for (Entity e : entities)
        //{
        //    const auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(e);
        //    auto& renderer = m_componentManager->GetComponent<NE::ECS::Component::Renderer>(e);

        //    const Mat4& modelMatrix = transform.modelMatrix;

        //    if (renderer.model && renderer.model->hasSphereBoundsLS)
        //    {
        //        const Vec3 centerLS = renderer.model->sphereCenterLS;
        //        const float radiusLS = renderer.model->sphereRadiusLS;

        //        renderer.visible = TestSphereFrustum(frustum, modelMatrix, centerLS, radiusLS);
        //    }
        //    else
        //    {
        //        renderer.visible = TestSphereFrustum(frustum, modelMatrix, Vec3(0.0f), 0.5f); // default
        //    }
        //}
    }

}
