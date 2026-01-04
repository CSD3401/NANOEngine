#pragma once

#include "ECS/Core/Entity.hpp"
#include "Math/Vec3.hpp"

namespace NE::ECS::Components {
	struct Collider;
	struct Rigidbody;
	struct Transform;
}

namespace NE::Physics {
	struct RaycastHit {
		Math::Vec3 point{ 0.f, 0.f, 0.f };
		Math::Vec3 normal{ 0.f, 0.f, 0.f };
		NE::ECS::Components::Collider* collder = nullptr;		// not yet implemented
		NE::ECS::Components::Rigidbody* rigidbody = nullptr;	// not yet implemented
		NE::ECS::Components::Transform* transform = nullptr;	// not yet implemented
		float distance = 0.f;
		NE::ECS::Entity colliderEntityID = NE::ECS::NO_ENTITY;
	};
}
