#pragma once

#include "Core/Reflection.hpp"
#include "Math/Vec3.hpp"

namespace NE::ECS::Component {
	struct Rigidbody {
		float mass{ 1.0f };
		float linearDamping{ 0.f };
		float angularDamping{ 0.05f };
		bool useGravity = true;
		bool isKinematic = false;

		bool freezeRotX = false;
		bool freezeRotY = false;
		bool freezeRotZ = false;

		bool freezePosX = false;
		bool freezePosY = false;
		bool freezePosZ = false;

		uint64_t luid;

		// Dirty flag for editor changes
		bool isDirty = false;

		NE_REFLECT_BEGIN(Rigidbody)
			NE_REFLECT_FIELD_NAMED(mass, "Mass"),
			NE_REFLECT_FIELD_NAMED(linearDamping, "Linear Damping"),
			NE_REFLECT_FIELD_NAMED(angularDamping, "Angular Damping"),
			NE_REFLECT_FIELD_NAMED(useGravity, "Use Gravity"),
			NE_REFLECT_FIELD_NAMED(isKinematic, "Is Kinematic"),
			NE_REFLECT_FIELD_HIDDEN(freezeRotX),
			NE_REFLECT_FIELD_HIDDEN(freezeRotY),
			NE_REFLECT_FIELD_HIDDEN(freezeRotZ),
			NE_REFLECT_FIELD_HIDDEN(freezePosX),
			NE_REFLECT_FIELD_HIDDEN(freezePosY),
			NE_REFLECT_FIELD_HIDDEN(freezePosZ),
			NE_REFLECT_FIELD_HIDDEN(luid)
			NE_REFLECT_END()
	};
}