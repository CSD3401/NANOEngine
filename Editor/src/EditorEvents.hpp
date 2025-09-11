#pragma once
#include <string>

namespace Editor {

	struct CreateEntityEvent {};
	struct DeleteEntityEvent {
		uint32_t deletedEntity;
	};
	struct SelectEntityEvent {
		uint32_t selectedEntity;
	};
}