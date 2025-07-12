#pragma once

#include <string>
#include "../../Core/Reflection.hpp"

namespace NANOEngine::ECS::Component {

	struct Name {
		// Exposed
		std::string name; // Name of the entity

		NE_REFLECT_BEGIN(Name)
			NE_REFLECT_FIELD(name)
		NE_REFLECT_END()
	};

}
