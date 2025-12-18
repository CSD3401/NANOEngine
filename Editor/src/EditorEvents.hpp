#pragma once
#include <string>

namespace Editor {

	struct CreateEntityEvent {};
	struct CreateUICanvasEntityEvent {};
	struct CreateUIImageEntityEvent {
		uint32_t parentCanvas;  // Which canvas to parent to
	};
	struct CreateUIButtonEntityEvent {
		uint32_t parentCanvas;  // Which canvas to parent to
	};
	struct DeleteEntityEvent {
		uint32_t deletedEntity;
	};
	struct SelectEntityEvent {
		uint32_t selectedEntity;
	};
}