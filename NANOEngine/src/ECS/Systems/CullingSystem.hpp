#ifndef CULLING_SYSTEM_HPP
#define CULLING_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../../Graphics/Core/Frustum.hpp"
#include "../../Graphics/Core/Camera.hpp"

namespace NE::ECS::Systems {

    class CullingSystem final : public System {
    public:
        explicit CullingSystem(ComponentManager* cm);

        void OnEntityAdded(Entity) override {}
        void OnEntityRemoved(Entity) override {}

        void Init() override {}
        void Update(double deltaTime) override; // runs before RenderSystem
        void Exit() override {}

    private:
        ComponentManager* m_componentManager = nullptr;

        static NE::Graphics::Frustum BuildFrustum();

        static bool VisibleSphere(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, float RadiusLS);

        static bool VisibleAABB(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, const NE::Math::Vec3& minLS, const NE::Math::Vec3& maxLS);
    };

} // namespace NE::ECS::Systems
#endif // END CULLING_SYSTEM_HPP
