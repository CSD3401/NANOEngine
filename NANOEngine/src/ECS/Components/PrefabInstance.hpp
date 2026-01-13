#ifndef PREFAB_INSTANCE_HPP
#define PREFAB_INSTANCE_HPP

#include <string>

#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	// Nothing much for now
	struct PrefabInstance {
		std::string prefabUUID = "";

		bool isDirty = false;

		NE_REFLECT_BEGIN(PrefabInstance)
			NE_REFLECT_FIELD(prefabUUID)
		NE_REFLECT_END()
	};
}

#endif