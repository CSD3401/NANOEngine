#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../../Graphics/Core/Frustum.hpp"
#include "../../Graphics/Core/Camera.hpp"

namespace NE::ECS::Systems {

    class RenderSystem final : public System {
    public:
		explicit RenderSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void RenderPicking();
		void Exit() override;

    private:
        ComponentManager* m_componentManager;

		static NE::Graphics::Frustum BuildFrustum();

		static bool TestSphereFrustum(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, float RadiusLS);
		static bool TestSphereFrustum(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, const NE::Math::Vec3& centerLS, float radiusLS); // overloaded TestSphereFrustum() for entities with no collider
		static bool TestAABBFrustum(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, const NE::Math::Vec3& minLS, const NE::Math::Vec3& maxLS);

		void FrustumCulling();
    };


}


