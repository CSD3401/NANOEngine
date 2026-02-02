#pragma once

#include <string>
#include <vector>

namespace Editor::Events {

	struct CreateEmptyEntityEvent {
		uint32_t parentEntity;
	};
	struct CreateCubeEntityEvent {
		uint32_t parentEntity;
	};
	struct CreateSphereEntityEvent {
		uint32_t parentEntity;
	};
	struct CreateCapsuleEntityEvent {
		uint32_t parentEntity;
	};
	struct CreateCylinderEntityEvent {
		uint32_t parentEntity;
	};
	struct CreatePlaneEntityEvent {
		uint32_t parentEntity;
	};
	struct CreateQuadEntityEvent {
		uint32_t parentEntity;
	};

	struct CreateDirectionalLightEvent {
		uint32_t parentEntity;
	};

	struct CreatePointLightEvent {
		uint32_t parentEntity;
	};

	struct CreateSpotLightEvent {
		uint32_t parentEntity;
	};

	// UI Creation Events (Unity-like workflow)
	struct CreateUICanvasEvent {};
	struct CreateUITextEvent { uint32_t parentEntity; };
	struct CreateUIImageEvent { uint32_t parentEntity; };
	struct CreateUIButtonEvent { uint32_t parentEntity; };
	struct CreateUIPanelEvent { uint32_t parentEntity; };

	struct HierarchyChangeEvent {
		uint32_t childEntity;
		uint32_t newParentEntity;
		int insertIndex;
	};

	struct DeleteEntityEvent {
		std::vector<uint32_t> entitiesToBeDeleted;
		uint32_t oldParentEntity;
	};

	struct HideCursorEvent {};
	struct ShowCursorEvent {};


	struct SelectEntityEvent {
		uint32_t selectedEntity;
	};

	struct GotoAssetPathEvent {
		std::string assetPath;
	};

	struct SceneChangedEvent {};

	struct AutoKeyRecordEvent {
		uint32_t componentTypeId; 
		uint32_t fieldId;
	};
}