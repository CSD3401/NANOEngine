#pragma once

#include "../../Core/Reflection.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

namespace NANOEngine::ECS::Component {

	struct Rigidbody {
		JPH::BodyID bodyId;

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
	};

}
