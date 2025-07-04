#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"

namespace NANOEngine::ECS {

	struct Transform {
		// Exposed
		Math::Vec3 position{ 0.f };
		Math::Vec3 scale{ 1.f };
		Math::Vec3 rotation{ 0.f };

		// Hidden
		bool isDirty = true;
		Math::Mat4 modelMatrix{};
		Math::Mat4 parent{};

		NE_REFLECT_BEGIN(Transform)
			NE_REFLECT_FIELD(position),
			NE_REFLECT_FIELD(scale),
			NE_REFLECT_FIELD(rotation)
		NE_REFLECT_END()
	};

}