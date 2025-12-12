#ifndef NANOENGINE_ECS_ENTITY_HPP
#define NANOENGINE_ECS_ENTITY_HPP

#include <cstdint>

namespace NE::ECS {

	using Entity = uint32_t;
	static constexpr Entity MAX_ENTITIES = 2000;
	static constexpr Entity NO_ENTITY = UINT32_MAX;

}

#endif // !NANOENGINE_ECS_ENTITY_HPP