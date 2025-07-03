#pragma once
#include <string>

namespace Editor {

	struct CreateEntityEvent {};
	struct DeleteEntityEvent {
		uint32_t deletedEntity;
	};
}