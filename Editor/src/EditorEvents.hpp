#pragma once
#include <string>

namespace Editor::Events {

	struct CreateEmptyEntityEvent {};
	struct CreateCubeEntityEvent {};
	struct CreateSphereEntityEvent {};
	struct CreateCapsuleEntityEvent {};
	struct CreateCylinderEntityEvent {};
	struct CreatePlaneEntityEvent {};

	struct DeleteEntityEvent {
		uint32_t deletedEntity;
	};


	struct CreateUICanvasEntityEvent {};
	struct CreateUIImageEntityEvent {
		uint32_t parentCanvas;  // Which canvas to parent to
	};
	struct SelectEntityEvent {
		uint32_t selectedEntity;
	};
}