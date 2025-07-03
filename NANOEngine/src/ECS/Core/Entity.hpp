#ifndef NANOENGINE_ECS_ENTITY_HPP
#define NANOENGINE_ECS_ENTITY_HPP

#include <cstdint>

namespace NANOEngine::ECS {

	using Entity = uint32_t;
	static constexpr Entity MAX_ENTITIES = 2000;
	static constexpr Entity NO_ENTITY = std::numeric_limits<Entity>::max();

}

#endif // !NANOENGINE_ECS_ENTITY_HPP