#pragma once
#include "EngineAPI.hpp"
#include <array>

/**
 * MirrorPuzzle - Tile-based puzzle with mirrored movement
 *
 * NOTE: Since we can't query entity names and child order is unreliable,
 * tiles are assigned manually via exposed TransformRef fields.
 *
 * Setup in editor:
 * 1. Assign all 12 original tiles to tile00, tile01, ... tile23
 * 2. Assign all 12 mirror tiles to mirrorTile00, ... mirrorTile23
 * 3. Assign targetTransform and mirrorTargetTransform
 */
class MirrorPuzzle : public IScript {
public:
	struct TileConfig {
		int row = 0;
		int col = 0;
		bool allowUp = true;
		bool allowDown = true;
		bool allowLeft = true;
		bool allowRight = true;

		NE_REFLECT_BEGIN(TileConfig)
			NE_REFLECT_FIELD(row),
			NE_REFLECT_FIELD(col),
			NE_REFLECT_FIELD(allowUp),
			NE_REFLECT_FIELD(allowDown),
			NE_REFLECT_FIELD(allowLeft),
			NE_REFLECT_FIELD(allowRight)
			NE_REFLECT_END()
	};

	MirrorPuzzle() {
		SCRIPT_COMPONENT_REF(targetTransform, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTargetTransform, TransformRef);

		// Original grid tiles (row 0)
		SCRIPT_COMPONENT_REF(tile00, TransformRef);
		SCRIPT_COMPONENT_REF(tile01, TransformRef);
		SCRIPT_COMPONENT_REF(tile02, TransformRef);
		SCRIPT_COMPONENT_REF(tile03, TransformRef);
		// Row 1
		SCRIPT_COMPONENT_REF(tile10, TransformRef);
		SCRIPT_COMPONENT_REF(tile11, TransformRef);
		SCRIPT_COMPONENT_REF(tile12, TransformRef);
		SCRIPT_COMPONENT_REF(tile13, TransformRef);
		// Row 2
		SCRIPT_COMPONENT_REF(tile20, TransformRef);
		SCRIPT_COMPONENT_REF(tile21, TransformRef);
		SCRIPT_COMPONENT_REF(tile22, TransformRef);
		SCRIPT_COMPONENT_REF(tile23, TransformRef);

		// Mirror grid tiles (row 0)
		SCRIPT_COMPONENT_REF(mirrorTile00, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile01, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile02, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile03, TransformRef);
		// Row 1
		SCRIPT_COMPONENT_REF(mirrorTile10, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile11, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile12, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile13, TransformRef);
		// Row 2
		SCRIPT_COMPONENT_REF(mirrorTile20, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile21, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile22, TransformRef);
		SCRIPT_COMPONENT_REF(mirrorTile23, TransformRef);
	}

	~MirrorPuzzle() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {
		SCRIPT_FIELD(startRow, Int);
		SCRIPT_FIELD(startCol, Int);
		SCRIPT_FIELD(endRow, Int);
		SCRIPT_FIELD(endCol, Int);
		SCRIPT_FIELD(zOffset, Float);
	}

	void Start() override {
		LOG_INFO("=== MirrorPuzzle Started ===");

		startRow = Clamp(startRow, 0, 2);
		startCol = Clamp(startCol, 0, 3);
		endRow = Clamp(endRow, 0, 2);
		endCol = Clamp(endCol, 0, 3);

		mirrorStartRow = startRow;
		mirrorStartCol = 3 - startCol;
		mirrorEndRow = endRow;
		mirrorEndCol = 3 - endCol;

		LOG_INFO("Original: Start(" << startRow << "," << startCol << ") -> End(" << endRow << "," << endCol << ")");
		LOG_INFO("Mirror:   Start(" << mirrorStartRow << "," << mirrorStartCol << ") -> End(" << mirrorEndRow << "," << mirrorEndCol << ")");

		// Cache tiles into array (manual assignment)
		tileTransforms[0] = tile00;
		tileTransforms[1] = tile01;
		tileTransforms[2] = tile02;
		tileTransforms[3] = tile03;
		tileTransforms[4] = tile10;
		tileTransforms[5] = tile11;
		tileTransforms[6] = tile12;
		tileTransforms[7] = tile13;
		tileTransforms[8] = tile20;
		tileTransforms[9] = tile21;
		tileTransforms[10] = tile22;
		tileTransforms[11] = tile23;

		mirrorTileTransforms[0] = mirrorTile00;
		mirrorTileTransforms[1] = mirrorTile01;
		mirrorTileTransforms[2] = mirrorTile02;
		mirrorTileTransforms[3] = mirrorTile03;
		mirrorTileTransforms[4] = mirrorTile10;
		mirrorTileTransforms[5] = mirrorTile11;
		mirrorTileTransforms[6] = mirrorTile12;
		mirrorTileTransforms[7] = mirrorTile13;
		mirrorTileTransforms[8] = mirrorTile20;
		mirrorTileTransforms[9] = mirrorTile21;
		mirrorTileTransforms[10] = mirrorTile22;
		mirrorTileTransforms[11] = mirrorTile23;

		// Initialize all tiles to allow all directions
		for (auto& row : grid) {
			row.fill(ALL);
		}
		for (auto& row : mirrorGrid) {
			row.fill(ALL);
		}

		// Apply tile restrictions for original grid
		for (const auto& restriction : tileRestrictions) {
			if (restriction.row >= 0 && restriction.row < 3 && restriction.col >= 0 && restriction.col < 4) {
				Direction allowed = NONE;
				if (restriction.allowUp) allowed = static_cast<Direction>(allowed | UP);
				if (restriction.allowDown) allowed = static_cast<Direction>(allowed | DOWN);
				if (restriction.allowLeft) allowed = static_cast<Direction>(allowed | LEFT);
				if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

				grid[restriction.row][restriction.col] = allowed;
				LOG_INFO("Original tile (" << restriction.row << "," << restriction.col << ") restricted to: " << static_cast<int>(allowed));

				// Update direction indicators (child 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT)
				int tileIndex = restriction.row * 4 + restriction.col;
				if (tileTransforms[tileIndex].IsValid()) {
					Entity tileEntity = tileTransforms[tileIndex].GetEntity();

					Entity upIndicator = GetChildOf(tileEntity, 0);
					Entity rightIndicator = GetChildOf(tileEntity, 1);
					Entity downIndicator = GetChildOf(tileEntity, 2);
					Entity leftIndicator = GetChildOf(tileEntity, 3);

					if (upIndicator != 0) SetActive(false, upIndicator);
					if (rightIndicator != 0) SetActive(false, rightIndicator);
					if (downIndicator != 0) SetActive(false, downIndicator);
					if (leftIndicator != 0) SetActive(false, leftIndicator);

					if (upIndicator != 0) SetActive((allowed & UP) != 0, upIndicator);
					if (rightIndicator != 0) SetActive((allowed & RIGHT) != 0, rightIndicator);
					if (downIndicator != 0) SetActive((allowed & DOWN) != 0, downIndicator);
					if (leftIndicator != 0) SetActive((allowed & LEFT) != 0, leftIndicator);
				}
			}
		}

		// Apply tile restrictions for mirror grid
		for (const auto& restriction : mirrorTileRestrictions) {
			if (restriction.row >= 0 && restriction.row < 3 && restriction.col >= 0 && restriction.col < 4) {
				Direction allowed = NONE;
				if (restriction.allowUp) allowed = static_cast<Direction>(allowed | UP);
				if (restriction.allowDown) allowed = static_cast<Direction>(allowed | DOWN);
				if (restriction.allowLeft) allowed = static_cast<Direction>(allowed | LEFT);
				if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

				mirrorGrid[restriction.row][restriction.col] = allowed;
				LOG_INFO("Mirror tile (" << restriction.row << "," << restriction.col << ") restricted to: " << static_cast<int>(allowed));

				// Update direction indicators
				int tileIndex = restriction.row * 4 + restriction.col;
				if (mirrorTileTransforms[tileIndex].IsValid()) {
					Entity tileEntity = mirrorTileTransforms[tileIndex].GetEntity();

					Entity upIndicator = GetChildOf(tileEntity, 0);
					Entity rightIndicator = GetChildOf(tileEntity, 1);
					Entity downIndicator = GetChildOf(tileEntity, 2);
					Entity leftIndicator = GetChildOf(tileEntity, 3);

					if (upIndicator != 0) SetActive(false, upIndicator);
					if (rightIndicator != 0) SetActive(false, rightIndicator);
					if (downIndicator != 0) SetActive(false, downIndicator);
					if (leftIndicator != 0) SetActive(false, leftIndicator);

					if (upIndicator != 0) SetActive((allowed & UP) != 0, upIndicator);
					if (rightIndicator != 0) SetActive((allowed & RIGHT) != 0, rightIndicator);
					if (downIndicator != 0) SetActive((allowed & DOWN) != 0, downIndicator);
					if (leftIndicator != 0) SetActive((allowed & LEFT) != 0, leftIndicator);
				}
			}
		}

		currentRow = startRow;
		currentCol = startCol;
		mirrorRow = mirrorStartRow;
		mirrorCol = mirrorStartCol;

		if (targetTransform.IsValid()) {
			int startTileIndex = startRow * 4 + startCol;
			if (tileTransforms[startTileIndex].IsValid()) {
				Vec3 startPos = GetTileWorldPosition(tileTransforms[startTileIndex]);
				startPos.z += zOffset;
				SetPosition(targetTransform, startPos);
				LOG_INFO("Original target placed at (" << currentRow << "," << currentCol << ")");
			}
		}

		if (mirrorTargetTransform.IsValid()) {
			int mirrorStartTileIndex = mirrorStartRow * 4 + mirrorStartCol;
			if (mirrorTileTransforms[mirrorStartTileIndex].IsValid()) {
				Vec3 mirrorStartPos = GetTileWorldPosition(mirrorTileTransforms[mirrorStartTileIndex]);
				mirrorStartPos.z += zOffset;
				SetPosition(mirrorTargetTransform, mirrorStartPos);
				LOG_INFO("Mirror target placed at (" << mirrorRow << "," << mirrorCol << ")");
			}
		}

		puzzleSolved = false;
		LogCurrentState();
	}

	void Update(double deltaTime) override {
		if (!targetTransform.IsValid() || !mirrorTargetTransform.IsValid()) return;
		if (puzzleSolved) return;

		if (Input::WasKeyPressed('W')) TryMoveUp();
		if (Input::WasKeyPressed('S')) TryMoveDown();
		if (Input::WasKeyPressed('A')) TryMoveLeft();
		if (Input::WasKeyPressed('D')) TryMoveRight();

		if (HasReachedEnd() && HasMirrorReachedEnd()) {
			if (!puzzleSolved) {
				puzzleSolved = true;
				LOG_INFO("\n=== PUZZLE SOLVED! ===");
				Events::Send(eventName.c_str());
			}
		}
	}

	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

	const char* GetTypeName() const override {
		return "MirrorPuzzle";
	}

	enum Direction : uint8_t {
		NONE = 0,
		UP = 1 << 0,
		DOWN = 1 << 1,
		LEFT = 1 << 2,
		RIGHT = 1 << 3,
		ALL = UP | DOWN | LEFT | RIGHT
	};

	int Clamp(int val, int min, int max) const {
		if (val < min) return min;
		if (val > max) return max;
		return val;
	}

	Vec3 GetTileWorldPosition(const TransformRef& tileRef) const {
		if (!tileRef.IsValid()) {
			return Vec3(0, 0, 0);
		}
		return GetPosition(tileRef);
	}

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
		bool mirrorMoved = TryMoveMirror(RIGHT, 0, 1);
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	void TryMoveRight() {
		LOG_INFO("\n--- Attempting RIGHT ---");
		bool originalMoved = TryMoveOriginal(RIGHT, 0, 1);
		bool mirrorMoved = TryMoveMirror(LEFT, 0, -1);
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

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
			Vec3 tilePos = GetTileWorldPosition(tileTransforms[tileIndex]);
			tilePos.z += zOffset;
			SetPosition(targetTransform, tilePos);
		}
	}

	void MoveMirrorTargetToTile(int tileIndex) {
		if (mirrorTileTransforms[tileIndex].IsValid() && mirrorTargetTransform.IsValid()) {
			Vec3 tilePos = GetTileWorldPosition(mirrorTileTransforms[tileIndex]);
			tilePos.z += zOffset;
			SetPosition(mirrorTargetTransform, tilePos);
		}
	}

	bool HasReachedEnd() const {
		return (currentRow == endRow && currentCol == endCol);
	}

	bool HasMirrorReachedEnd() const {
		return (mirrorRow == mirrorEndRow && mirrorCol == mirrorEndCol);
	}

	void LogCurrentState() const {
		LOG_INFO("=== Current State ===");
		LOG_INFO("Original: (" << currentRow << "," << currentCol << ") -> Goal(" << endRow << "," << endCol << ")");
		LOG_INFO("Mirror:   (" << mirrorRow << "," << mirrorCol << ") -> Goal(" << mirrorEndRow << "," << mirrorEndCol << ")");
	}

	// Original grid tiles
	TransformRef tile00, tile01, tile02, tile03;
	TransformRef tile10, tile11, tile12, tile13;
	TransformRef tile20, tile21, tile22, tile23;

	// Mirror grid tiles
	TransformRef mirrorTile00, mirrorTile01, mirrorTile02, mirrorTile03;
	TransformRef mirrorTile10, mirrorTile11, mirrorTile12, mirrorTile13;
	TransformRef mirrorTile20, mirrorTile21, mirrorTile22, mirrorTile23;

	TransformRef targetTransform;
	TransformRef mirrorTargetTransform;

	int startRow = 2;
	int startCol = 0;
	int endRow = 0;
	int endCol = 3;

	float zOffset = 1.0f;

	std::vector<TileConfig> tileRestrictions;
	std::vector<TileConfig> mirrorTileRestrictions;

	std::string eventName = "MirrorPuzzleSolved";

	int mirrorStartRow = 0;
	int mirrorStartCol = 0;
	int mirrorEndRow = 0;
	int mirrorEndCol = 0;

	int currentRow = 0;
	int currentCol = 0;
	int mirrorRow = 0;
	int mirrorCol = 0;

	bool puzzleSolved = false;

	std::array<std::array<Direction, 4>, 3> grid;
	std::array<std::array<Direction, 4>, 3> mirrorGrid;

	std::array<TransformRef, 12> tileTransforms;
	std::array<TransformRef, 12> mirrorTileTransforms;
};