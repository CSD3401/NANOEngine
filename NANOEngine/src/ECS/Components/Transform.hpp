#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"
#include <vector> 

namespace NE::ECS::Component {

	inline constexpr uint32_t INVALID_PARENT = UINT32_MAX;

	struct Transform {

		Math::Vec3 localPosition{ 0.f, 0.f, 0.f };
		Math::Vec3 localScale{ 1.f, 1.f, 1.f };
		Math::Vec3 localRotationEuler{ 0.f, 0.f,0.f };

		uint64_t luid = 0;

		uint32_t parent = INVALID_PARENT;
		uint64_t parentLuid = 0;
		std::vector<uint32_t> children{};

		bool isDirty = true;
		Math::Mat4 localMatrix{};
		Math::Mat4 worldMatrix{};


		NE_REFLECT_BEGIN(Transform)
			NE_REFLECT_FIELD_NAMED(localPosition,		"Position"),
			NE_REFLECT_FIELD_NAMED(localScale,			"Scale"),
			NE_REFLECT_FIELD_NAMED(localRotationEuler,	"Rotation"),
			NE_REFLECT_FIELD_HIDDEN(luid),
			NE_REFLECT_FIELD_HIDDEN(parentLuid)
		NE_REFLECT_END()
	};

}