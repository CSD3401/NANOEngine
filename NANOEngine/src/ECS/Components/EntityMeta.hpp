#pragma once
#include <string>
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	struct EntityMeta {
		std::string name = "Empty Entity";
		uint64_t luid = 0;
		bool isActive = true;

		NE_REFLECT_BEGIN(EntityMeta)
			NE_REFLECT_FIELD(name),
			NE_REFLECT_FIELD(isActive),
			NE_REFLECT_FIELD_HIDDEN(luid)
		NE_REFLECT_END()
	};
}