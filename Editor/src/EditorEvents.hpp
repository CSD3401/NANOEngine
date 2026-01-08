#pragma once
//#include <string>
#include <vector>

namespace Editor::Events {

	struct CreateEmptyEntityEvent {};

	struct CreateCubeEntityEvent {};
	struct CreateSphereEntityEvent {};
	struct CreateCapsuleEntityEvent {};
	struct CreateCylinderEntityEvent {};
	struct CreatePlaneEntityEvent {};

	struct CreateCanvasEntityEvent {};

	struct DeleteEntityEvent {
		std::vector<uint32_t> entitiesToBeDeleted;
	};


	struct CreateUICanvasEntityEvent {};
	struct CreateUIImageEntityEvent {
		uint32_t parentCanvas;  // Which canvas to parent to
	};
	struct SelectEntityEvent {
		uint32_t selectedEntity;
	};
}