#pragma once
#include "EngineAPI.hpp"
#include <array>

/**
 * MirrorPuzzle - Refactored version of MatchingPuzzle
 *
 * Instead of 28 individual TransformRefs, we use just 6:
 *   - targetTransform, endTransform, gridParent (original)
 *   - mirrorTargetTransform, mirrorEndTransform, mirrorGridParent (mirror)
 *
 * The 12 tiles for each grid are obtained via GetChildOf() from the parent entities.
 *
 * ============================================================================
 * REQUIRED ENGINE UPDATE:
 * ============================================================================
 *
 * Add these functions to IScript (see ScriptAPI_additions.h/.cpp):
 *   - size_t GetChildCountOf(Entity entity) const;
 *   - Entity GetChildOf(Entity entity, size_t index) const;
 *
 * ============================================================================
 * HIERARCHY SETUP:
 * ============================================================================
 *
 *   Scene
 *   ├── ScriptHolder (attach this script here - can be any entity)
 *   ├── OriginalGridParent (assign to gridParent)
 *   │   ├── Tile0 (child 0)
 *   │   ├── Tile1 (child 1)
 *   │   └── ... Tile11 (child 11)
 *   ├── MirrorGridParent (assign to mirrorGridParent)
 *   │   ├── Tile0 (child 0)
 *   │   ├── Tile1 (child 1)
 *   │   └── ... Tile11 (child 11)
 *   ├── Target (assign to targetTransform)
 *   ├── MirrorTarget (assign to mirrorTargetTransform)
 *   ├── EndGoal (assign to endTransform)
 *   └── MirrorEndGoal (assign to mirrorEndTransform)
 *
 * Grid Layout (4 columns x 3 rows):
 *   Row 0: [0] [1] [2] [3]
 *   Row 1: [4] [5] [6] [7]
 *   Row 2: [8] [9] [10][11]
 */
class MirrorPuzzle : public IScript {
public:
	MirrorPuzzle() {
		// Only 6 TransformRefs instead of 28!
		SCRIPT_COMPONENT_REF(targetTransform, Transform);
		SCRIPT_COMPONENT_REF(endTransform, Transform);
		SCRIPT_COMPONENT_REF(gridParent, Transform);

		SCRIPT_COMPONENT_REF(mirrorTargetTransform, Transform);
		SCRIPT_COMPONENT_REF(mirrorEndTransform, Transform);
		SCRIPT_COMPONENT_REF(mirrorGridParent, Transform);

		SCRIPT_FIELD(tileSpacingX, Float);
		SCRIPT_FIELD(tileSpacingY, Float);
	}

	~MirrorPuzzle() override = default;

	void Awake() override {}
	void Initialize(Entity entity) override {}

	void Start() override {
		LOG_INFO("=== UPDATED MirrorPuzzle Started ===");

		// Validate parent refs
		if (!gridParent.IsValid()) {
			LOG_ERROR("gridParent is not assigned!");
			return;
		}
		if (!mirrorGridParent.IsValid()) {
			LOG_ERROR("mirrorGridParent is not assigned!");
			return;
		}

		// Get the 12 children of gridParent using GetChildOf()
		Entity gridParentEntity = gridParent.GetEntity();
		size_t originalChildCount = GetChildCountOf(gridParentEntity);
		LOG_INFO("Original grid parent has " << originalChildCount << " children");

		if (originalChildCount < 12) {
			LOG_ERROR("Original grid needs 12 tile children, found " << originalChildCount);
			return;
		}

		for (size_t i = 0; i < 12; ++i) {
			Entity childEntity = GetChildOf(gridParentEntity, i);
			tileTransforms[i] = GetTransformRef(childEntity);
		}
		LOG_INFO("Cached 12 original tile transforms");

		// Get the 12 children of mirrorGridParent using GetChildOf()
		Entity mirrorGridParentEntity = mirrorGridParent.GetEntity();
		size_t mirrorChildCount = GetChildCountOf(mirrorGridParentEntity);
		LOG_INFO("Mirror grid parent has " << mirrorChildCount << " children");

		if (mirrorChildCount < 12) {
			LOG_ERROR("Mirror grid needs 12 tile children, found " << mirrorChildCount);
			return;
		}

		for (size_t i = 0; i < 12; ++i) {
			Entity childEntity = GetChildOf(mirrorGridParentEntity, i);
			mirrorTileTransforms[i] = GetTransformRef(childEntity);
		}
		LOG_INFO("Cached 12 mirror tile transforms");

		// Initialize direction grids - all tiles allow all directions
		for (auto& row : grid) {
			row.fill(ALL);
		}
		for (auto& row : mirrorGrid) {
			row.fill(ALL);
		}

		// Start positions (logical grid coordinates)
		// Original starts at bottom-left (2,0)
		// Mirror starts at bottom-right (2,3) - horizontally mirrored
		currentRow = 2;
		currentCol = 0;
		mirrorRow = 2;
		mirrorCol = 3;

		// Position targets on starting tiles using WORLD position
		// Original: tile index = 2*4 + 0 = 8
		if (targetTransform.IsValid() && tileTransforms[8].IsValid()) {
			Vec3 startPos = GetTileWorldPosition(tileTransforms[8], gridParent);
			startPos.z += 1.0f;
			SetPosition(targetTransform, startPos);
			LOG_INFO("Original target placed at (" << currentRow << "," << currentCol << ")");
		}

		// Mirror: tile index = 2*4 + 3 = 11 on the MIRROR grid
		if (mirrorTargetTransform.IsValid() && mirrorTileTransforms[mirrorRow * 4 + mirrorCol].IsValid()) {
			Vec3 mirrorStartPos = GetTileWorldPosition(mirrorTileTransforms[mirrorRow * 4 + mirrorCol], mirrorGridParent);
			mirrorStartPos.z += 1.0f;
			SetPosition(mirrorTargetTransform, mirrorStartPos);
			LOG_INFO("Mirror target placed at (" << mirrorRow << "," << mirrorCol << ") on mirror grid");
		}

		LogCurrentState();
	}

	void Update(double deltaTime) override {
		if (!targetTransform.IsValid() || !mirrorTargetTransform.IsValid()) return;

		if (Input::WasKeyPressed('W')) TryMoveUp();
		if (Input::WasKeyPressed('S')) TryMoveDown();
		if (Input::WasKeyPressed('A')) TryMoveLeft();
		if (Input::WasKeyPressed('D')) TryMoveRight();
		if (Input::WasKeyPressed('P')) PrintGridState();

		if (HasReachedEnd() && HasMirrorReachedEnd()) {
			static bool hasLogged = false;
			if (!hasLogged) {
				LOG_INFO("*** PUZZLE SOLVED! Both targets reached their goals! ***");
				hasLogged = true;
			}
		}
	}

	void OnDestroy() override {}
	void OnEnable() override {}
	void OnDisable() override {}
	void OnValidate() override {}

	const char* GetTypeName() const override { return "MirrorPuzzle"; }

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	enum Direction : uint8_t {
		NONE = 0b0000,
		UP = 0b0001,
		RIGHT = 0b0010,
		DOWN = 0b0100,
		LEFT = 0b1000,
		ALL = 0b1111
	};

	// === HELPER: Get world position of a tile ===
	// GetPosition returns local position, so we need to add parent's position
	Vec3 GetTileWorldPosition(TransformRef& tile, TransformRef& parent) {
		Vec3 localPos = GetPosition(tile);
		Vec3 parentPos = GetPosition(parent);
		return Vec3(localPos.x + parentPos.x, localPos.y + parentPos.y, localPos.z + parentPos.z);
	}

	// === MOVEMENT METHODS ===

	void TryMoveUp() {
		LOG_INFO("\n--- Attempting UP ---");
		bool originalMoved = TryMoveOriginal(UP, -1, 0);
		bool mirrorMoved = TryMoveMirror(UP, -1, 0);
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	void TryMoveDown() {
		LOG_INFO("\n--- Attempting DOWN ---");
		bool originalMoved = TryMoveOriginal(DOWN, 1, 0);
		bool mirrorMoved = TryMoveMirror(DOWN, 1, 0);
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	void TryMoveLeft() {
		LOG_INFO("\n--- Attempting LEFT ---");
		bool originalMoved = TryMoveOriginal(LEFT, 0, -1);
		bool mirrorMoved = TryMoveMirror(RIGHT, 0, 1);  // Mirror moves opposite!
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	void TryMoveRight() {
		LOG_INFO("\n--- Attempting RIGHT ---");
		bool originalMoved = TryMoveOriginal(RIGHT, 0, 1);
		bool mirrorMoved = TryMoveMirror(LEFT, 0, -1);  // Mirror moves opposite!
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	// === HELPER METHODS ===

	bool TryMoveOriginal(Direction dir, int rowDelta, int colDelta) {
		if (!CanMoveInDirection(grid[currentRow][currentCol], dir)) {
			LOG_WARNING("Original: Cannot move - current tile doesn't allow it");
			return false;
		}

		int newRow = currentRow + rowDelta;
		int newCol = currentCol + colDelta;

		if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
			LOG_WARNING("Original: Cannot move - would go out of bounds");
			return false;
		}

		Direction oppositeDir = GetOppositeDirection(dir);
		if (!CanMoveInDirection(grid[newRow][newCol], oppositeDir)) {
			LOG_WARNING("Original: Cannot move - destination doesn't allow entry");
			return false;
		}

		currentRow = newRow;
		currentCol = newCol;
		MoveOriginalTargetToTile(currentRow * 4 + currentCol);
		LOG_INFO("Original moved to (" << currentRow << "," << currentCol << ")");
		return true;
	}

	bool TryMoveMirror(Direction dir, int rowDelta, int colDelta) {
		if (!CanMoveInDirection(mirrorGrid[mirrorRow][mirrorCol], dir)) {
			LOG_WARNING("Mirror: Cannot move - current tile doesn't allow it");
			return false;
		}

		int newRow = mirrorRow + rowDelta;
		int newCol = mirrorCol + colDelta;

		if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
			LOG_WARNING("Mirror: Cannot move - would go out of bounds");
			return false;
		}

		Direction oppositeDir = GetOppositeDirection(dir);
		if (!CanMoveInDirection(mirrorGrid[newRow][newCol], oppositeDir)) {
			LOG_WARNING("Mirror: Cannot move - destination doesn't allow entry");
			return false;
		}

		mirrorRow = newRow;
		mirrorCol = newCol;
		MoveMirrorTargetToTile(mirrorRow * 4 + mirrorCol);
		LOG_INFO("Mirror moved to (" << mirrorRow << "," << mirrorCol << ")");
		return true;
	}

	bool CanMoveInDirection(Direction tile, Direction dir) const {
		return (static_cast<uint8_t>(tile) & static_cast<uint8_t>(dir)) != 0;
	}

	Direction GetOppositeDirection(Direction dir) const {
		switch (dir) {
		case UP:    return DOWN;
		case DOWN:  return UP;
		case LEFT:  return RIGHT;
		case RIGHT: return LEFT;
		default:    return NONE;
		}
	}

	void MoveOriginalTargetToTile(int tileIndex) {
		if (tileTransforms[tileIndex].IsValid() && targetTransform.IsValid()) {
			Vec3 tilePos = GetTileWorldPosition(tileTransforms[tileIndex], gridParent);
			tilePos.z += 1.0f;
			SetPosition(targetTransform, tilePos);
		}
	}

	void MoveMirrorTargetToTile(int tileIndex) {
		if (mirrorTileTransforms[tileIndex].IsValid() && mirrorTargetTransform.IsValid()) {
			Vec3 tilePos = GetTileWorldPosition(mirrorTileTransforms[tileIndex], mirrorGridParent);
			tilePos.z += 1.0f;
			SetPosition(mirrorTargetTransform, tilePos);
		}
	}

	bool HasReachedEnd() const {
		if (!endTransform.IsValid() || !targetTransform.IsValid()) return false;

		Vec3 targetPos = GetPosition(targetTransform);
		Vec3 endPos = GetPosition(endTransform);

		float dx = targetPos.x - endPos.x;
		float dy = targetPos.y - endPos.y;
		float dz = targetPos.z - endPos.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		return distance < 0.5f;
	}

	bool HasMirrorReachedEnd() const {
		if (!mirrorEndTransform.IsValid() || !mirrorTargetTransform.IsValid()) return false;

		Vec3 targetPos = GetPosition(mirrorTargetTransform);
		Vec3 endPos = GetPosition(mirrorEndTransform);

		float dx = targetPos.x - endPos.x;
		float dy = targetPos.y - endPos.y;
		float dz = targetPos.z - endPos.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		return distance < 0.5f;
	}

	void LogCurrentState() const {
		LOG_INFO("=== Current State ===");
		LOG_INFO("Original: (" << currentRow << "," << currentCol << ")");
		LOG_INFO("Mirror:   (" << mirrorRow << "," << mirrorCol << ")");
	}

	void PrintGridState() const {
		LOG_INFO("\n=== Grid State ===");
		LOG_INFO("ORIGINAL GRID:");
		for (int row = 0; row < 3; row++) {
			LOG_INFO("Row " << row << ": "
				<< static_cast<int>(grid[row][0]) << " "
				<< static_cast<int>(grid[row][1]) << " "
				<< static_cast<int>(grid[row][2]) << " "
				<< static_cast<int>(grid[row][3]));
		}
		LOG_INFO("Original position: (" << currentRow << "," << currentCol << ")");

		LOG_INFO("\nMIRROR GRID:");
		for (int row = 0; row < 3; row++) {
			LOG_INFO("Row " << row << ": "
				<< static_cast<int>(mirrorGrid[row][0]) << " "
				<< static_cast<int>(mirrorGrid[row][1]) << " "
				<< static_cast<int>(mirrorGrid[row][2]) << " "
				<< static_cast<int>(mirrorGrid[row][3]));
		}
		LOG_INFO("Mirror position: (" << mirrorRow << "," << mirrorCol << ")");
	}

	// === EXPOSED FIELDS (6 refs instead of 28!) ===

	// Original
	TransformRef targetTransform;
	TransformRef endTransform;
	TransformRef gridParent;  // Parent entity with 12 original tile children

	// Mirror
	TransformRef mirrorTargetTransform;
	TransformRef mirrorEndTransform;
	TransformRef mirrorGridParent;  // Parent entity with 12 mirror tile children

	float tileSpacingX = 1.5f;
	float tileSpacingY = 1.5f;

	// === INTERNAL STATE (populated from GetChildOf in Start) ===
	std::array<std::array<Direction, 4>, 3> grid;
	std::array<std::array<Direction, 4>, 3> mirrorGrid;

	std::array<TransformRef, 12> tileTransforms;       // Filled via GetChildOf(gridParent, 0-11)
	std::array<TransformRef, 12> mirrorTileTransforms; // Filled via GetChildOf(mirrorGridParent, 0-11)

	int currentRow = 0;
	int currentCol = 0;
	int mirrorRow = 0;
	int mirrorCol = 0;
};