#pragma once

#include "ECS/Core/Entity.hpp"
#include "Math/Vec3.hpp"

namespace NE::Physics {
	struct RaycastHit {
		Math::Vec3 point{ 0.f, 0.f, 0.f };
		Math::Vec3 normal{ 0.f, 0.f, 0.f };

		// LUID-based component references
		uint64_t colliderLuid = 0;    // LUID to Collider component
		uint64_t rigidbodyLuid = 0;   // LUID to Rigidbody (0 if none)
		uint64_t transformLuid = 0;   // LUID to Transform component

		float distance = 0.f;
		NE::ECS::Entity colliderEntityID = NE::ECS::NO_ENTITY;
	};
}
