#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../../Graphics/Core/Frustum.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"

namespace NE::Core {
	class LUIDRegistry;
}

namespace NE::ECS::Systems {

    class RenderSystem final : public System {
    public:
		explicit RenderSystem(ComponentManager* cm, Core::LUIDRegistry* lr);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

    private:
        ComponentManager* m_componentManager;
		Core::LUIDRegistry* m_luidRegistry;

		static NE::Graphics::Frustum BuildFrustum();

		static bool TestSphereFrustum(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, const NE::Math::Vec3& centerLS, float radiusLS);
		//static bool TestAABBFrustum(const NE::Graphics::Frustum& F, const NE::Math::Mat4& M, const NE::Math::Vec3& minLS, const NE::Math::Vec3& maxLS);

		void FrustumCulling();
    };


}


