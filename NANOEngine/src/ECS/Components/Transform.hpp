#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct Transform {
		Math::Vec3 localPosition{ 0.f, 0.f, 0.f };
		Math::Vec3 localScale{ 1.f, 1.f, 1.f };
		Math::Vec3 localRotationEuler{ 0.f, 0.f,0.f };

		uint32_t parent;
		uint64_t parentLUID;

		bool isDirty = false;
		Math::Mat4 localMatrix{};
		Math::Mat4 worldMatrix{};

		NE_REFLECT_BEGIN(Transform)
			NE_REFLECT_FIELD_NAMED(localPosition,		"Position"),
			NE_REFLECT_FIELD_NAMED(localScale,			"Scale"),
			NE_REFLECT_FIELD_NAMED(localRotationEuler,	"Rotation")
		NE_REFLECT_END()
	};

}