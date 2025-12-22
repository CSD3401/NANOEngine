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
		//uint8_t motionType = 2U;  // 0 = Static, 1 = Kinematic, 2 = Dynamic

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
			NE_REFLECT_FIELD(mass),
			NE_REFLECT_FIELD(linearDamping),
			NE_REFLECT_FIELD(angularDamping),
			NE_REFLECT_FIELD(useGravity),
			NE_REFLECT_FIELD(isKinematic),
			NE_REFLECT_FIELD_HIDDEN(freezeRotX),
			NE_REFLECT_FIELD_HIDDEN(freezeRotY),
			NE_REFLECT_FIELD_HIDDEN(freezeRotZ),
			NE_REFLECT_FIELD_HIDDEN(freezePosX),
			NE_REFLECT_FIELD_HIDDEN(freezePosY),
			NE_REFLECT_FIELD_HIDDEN(freezePosZ)
		NE_REFLECT_END()
	};

}
