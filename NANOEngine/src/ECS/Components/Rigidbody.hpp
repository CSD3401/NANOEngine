#pragma once

#include "../../Core/Reflection.hpp"
#include "../../Math/Vec3.hpp"

namespace NE::ECS::Component {

	struct Rigidbody {
		uint32_t bodyID;

		float mass{ 1.0f };
		uint8_t motionType = 2U;  // 0 = Static, 1 = Kinematic, 2 = Dynamic
		bool useGravity = true;

		// temp
		bool isStatic = false;

		Math::Vec3 initialVelocity{ 0.f, 0.f, 0.f };

		// Dirty flag for editor changes
		bool isDirty = false;

		NE_REFLECT_BEGIN(Rigidbody)
			NE_REFLECT_FIELD(mass),
			NE_REFLECT_FIELD(motionType),
			NE_REFLECT_FIELD(useGravity),
			NE_REFLECT_FIELD(isStatic),
			NE_REFLECT_FIELD(initialVelocity)
		NE_REFLECT_END()
	};

}
