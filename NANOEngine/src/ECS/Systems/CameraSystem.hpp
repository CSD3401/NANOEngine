#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include <optional>

// Forward declarations
namespace NE::ECS::Component {
	struct Camera; 
	struct Transform;
}
namespace NE::Math {
	struct Vec3;
	struct Mat4;
}

namespace NE::ECS::Systems {
	using NE::ECS::Entity;
	using NE::Math::Vec3;
	using NE::Math::Mat4;
	using NE::ECS::Component::Camera;
	using NE::ECS::Component::Transform;

	class CameraSystem final : public System {
	public:
		explicit CameraSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

	private:
		// Helper functions for camera management
		void BuildProjection(Camera& cam);
		void BuildView(Camera& cam, Transform& transform);		
		inline Vec3 ForwardFromEuler(const Vec3& euler);

		std::optional<Entity> m_mainCameraEntity; // Track main camera entity
		
		ComponentManager* m_componentManager;
	};
}