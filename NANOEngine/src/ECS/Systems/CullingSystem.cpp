#include "CullingSystem.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Renderer.hpp"
#include "../Components/Collider.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"

using namespace NE::Math;
using NE::Graphics::Frustum;
using NE::Graphics::GraphicsManager;

namespace NE::ECS::Systems {

    CullingSystem::CullingSystem(ComponentManager* cm) : m_componentManager(cm) {}

    Frustum CullingSystem::BuildFrustum() {
        auto* cam = GraphicsManager::GetCamera();

        if (!cam)
        {
            return Frustum::ExtractPlanesFromVP(Mat4{}); // default
        }

        const Mat4& V = cam->GetViewMatrix();
        const Mat4& P = cam->GetProjectionMatrix();

        Mat4 nonConstPCopy = P;
        return Frustum::ExtractPlanesFromVP(nonConstPCopy * V);
    }

    bool CullingSystem::VisibleSphere(const Frustum& F, const Mat4& M, float radiusLS) {
        // get the center in world space (translation portion)
        Vec3 centerWS = M.GetTranslation();

        // get the scaling factors
        Vec3 scale = M.GetScale();

        // compute the radius in world space using the largest scaling factor
        float radiusWS = radiusLS * std::max(scale.x, std::max(scale.y, scale.z));

        // test intersection of bounding sphere with frustum
        return F.IntersectsSphere(centerWS, radiusWS);
    }

    bool CullingSystem::VisibleAABB(const Frustum& F, const Mat4& M, const NE::Math::Vec3& minLS, const NE::Math::Vec3& maxLS) {
        // 8 local corners of the AABB
        const Vec3 cornersLS[8] = {
            {minLS.x, minLS.y, minLS.z},
            {maxLS.x, minLS.y, minLS.z},
            {minLS.x, maxLS.y, minLS.z},
            {maxLS.x, maxLS.y, minLS.z},
            {minLS.x, minLS.y, maxLS.z},
            {maxLS.x, minLS.y, maxLS.z},
            {minLS.x, maxLS.y, maxLS.z},
            {maxLS.x, maxLS.y, maxLS.z},
        };

        // helps to transform a point in local space to world space
        auto pointLSToWS = [](const Mat4& matrix, const Vec3& p) -> Vec3 {
            return Vec3{
                matrix.a[0] * p.x + matrix.a[4] * p.y + matrix.a[8] * p.z + matrix.a[12],
                matrix.a[1] * p.x + matrix.a[5] * p.y + matrix.a[9] * p.z + matrix.a[13],
                matrix.a[2] * p.x + matrix.a[6] * p.y + matrix.a[10] * p.z + matrix.a[14]
            };
            };

        // seed with first corner
        Vec3 minWS = pointLSToWS(M, cornersLS[0]);
        Vec3 maxWS = minWS;

        // continue with the remaining corners
        for (int i = 1; i < 8; ++i)
        {
            Vec3 cornerWS = pointLSToWS(M, cornersLS[i]);
            minWS = Vec3{ std::min(minWS.x, cornerWS.x), std::min(minWS.y, cornerWS.y), std::min(minWS.z, cornerWS.z) };
            maxWS = Vec3{ std::max(maxWS.x, cornerWS.x), std::max(maxWS.y, cornerWS.y), std::max(maxWS.z, cornerWS.z) };
        }

        // test intersection of AABB with frustum
        return F.IntersectsAABB(minWS, maxWS);
    }

    void CullingSystem::Update(double) {
        // build frustum from the active camera
        const Frustum F = BuildFrustum();

        const auto& entities = GetEntities();
        for (Entity e : entities)
        {
            const auto& transform = m_componentManager->GetComponent<NE::ECS::Component::Transform>(e);
            auto& renderer = m_componentManager->GetComponent<NE::ECS::Component::Renderer>(e);
            const auto& collider = m_componentManager->GetComponent<NE::ECS::Component::Collider>(e);

            bool isVisible = false;

            const Mat4& M = transform.modelMatrix;

            switch (collider.shapeType)
            {
            case NE::ECS::Component::Collider::ShapeType::Sphere:
            {
                isVisible = VisibleSphere(F, M, collider.radius);
            }
            break;

            case NE::ECS::Component::Collider::ShapeType::Box:
            {
                const Vec3 he = collider.halfExtents;
                const Vec3 minLS = Vec3{ -he.x, -he.y, -he.z };
                const Vec3 maxLS = Vec3{ he.x,  he.y,  he.z };
                isVisible = VisibleAABB(F, M, minLS, maxLS);
            }
            break;

            default:
            {
                isVisible = VisibleSphere(F, M, 0.5f); // default
            }
            break;
            }

            renderer.visible = isVisible;
        }
    }

} // namespace NE::ECS::Systems