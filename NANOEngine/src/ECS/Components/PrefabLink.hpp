#ifndef PREFAB_LINK_HPP
#define PREFAB_LINK_HPP

#include <string>

#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	struct PrefabLink {
		std::string uuid = "";
		uint32_t localID = 0;
		bool isRoot = false;

		NE_REFLECT_BEGIN(PrefabLink)
			NE_REFLECT_FIELD(uuid),
			NE_REFLECT_FIELD(localID),
			NE_REFLECT_FIELD_HIDDEN(isRoot)
		NE_REFLECT_END()
	};
}

#endif // !PREFAB_LINK_HPP
