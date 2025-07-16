#pragma once

#include "../../Core/Reflection.hpp"
#include "../../Math/Vec3.hpp"
//#include <Jolt/Jolt.h>
//#include <Jolt/Physics/Body/Body.h>

namespace NANOEngine::ECS::Component {

	struct Rigidbody {
		//JPH::BodyID bodyId;
		uint32_t bodyID;

		float mass{ 1.0f };
		uint8_t motionType = 2U;

		//JPH::EMotionType motionType{ JPH::EMotionType::Dynamic };
		Math::Vec3 initialVelocity{ 0.f, 0.f, 0.f };

		NE_REFLECT_BEGIN(Rigidbody)
			NE_REFLECT_FIELD(mass),
			//NE_REFLECT_FIELD(motionType),
			NE_REFLECT_FIELD(initialVelocity)
		NE_REFLECT_END()
	};

}
