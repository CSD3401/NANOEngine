#pragma once

#include "../../Core/Reflection.hpp"
#include "../../Math/Vec3.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

namespace NANOEngine::ECS::Component {

	struct Rigidbody {
		JPH::BodyID bodyId;

		float mass{ 1.0f };
		JPH::EMotionType motionType{ JPH::EMotionType::Dynamic };
		Math::Vec3 initialVelocity{ 0.f, 0.f, 0.f };

		//Graphics::Material material;
		// Exposed
		//std::filesystem::path modelPath;
		//std::filesystem::path materialPath;

		//// Internal
		//std::shared_ptr<Graphics::Model> model;
		//std::shared_ptr<Graphics::Material> material;

		//NE_REFLECT_BEGIN(Renderer)
		//	NE_REFLECT_FIELD(modelPath),
		//	NE_REFLECT_FIELD(materialPath)
		//	NE_REFLECT_END()
		NE_REFLECT_BEGIN(Rigidbody)
			NE_REFLECT_FIELD(mass),
			NE_REFLECT_FIELD(motionType),
			NE_REFLECT_FIELD(initialVelocity)
		NE_REFLECT_END()
	};

}
