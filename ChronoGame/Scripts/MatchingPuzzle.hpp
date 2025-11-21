#pragma once
#include "EngineAPI.hpp"
#include <array>

class MatchingPuzzle : public IScript {
public:
	MatchingPuzzle() {
		// Original target and grid
		SCRIPT_COMPONENT_REF(targetTransform, Transform);
		SCRIPT_COMPONENT_REF(endTransform, Transform);

		SCRIPT_COMPONENT_REF(transform0, Transform);
		SCRIPT_COMPONENT_REF(transform1, Transform);
		SCRIPT_COMPONENT_REF(transform2, Transform);
		SCRIPT_COMPONENT_REF(transform3, Transform);
		SCRIPT_COMPONENT_REF(transform4, Transform);
		SCRIPT_COMPONENT_REF(transform5, Transform);
		SCRIPT_COMPONENT_REF(transform6, Transform);
		SCRIPT_COMPONENT_REF(transform7, Transform);
		SCRIPT_COMPONENT_REF(transform8, Transform);
		SCRIPT_COMPONENT_REF(transform9, Transform);
		SCRIPT_COMPONENT_REF(transform10, Transform);
		SCRIPT_COMPONENT_REF(transform11, Transform);

		// Mirror target and grid
		SCRIPT_COMPONENT_REF(mirrorTargetTransform, Transform);
		SCRIPT_COMPONENT_REF(mirrorEndTransform, Transform);

		SCRIPT_COMPONENT_REF(mirrorTransform0, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform1, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform2, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform3, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform4, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform5, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform6, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform7, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform8, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform9, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform10, Transform);
		SCRIPT_COMPONENT_REF(mirrorTransform11, Transform);

		SCRIPT_FIELD(tileSpacingX, Float);
		SCRIPT_FIELD(tileSpacingY, Float);
	}

	~MatchingPuzzle() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {}

	void Start() override {
		LOG_INFO("=== MatchingPuzzle Started ===");

		// Initialize original grid transforms
		tileTransforms[0] = transform0;
		tileTransforms[1] = transform1;
		tileTransforms[2] = transform2;
		tileTransforms[3] = transform3;
		tileTransforms[4] = transform4;
		tileTransforms[5] = transform5;
		tileTransforms[6] = transform6;
		tileTransforms[7] = transform7;
		tileTransforms[8] = transform8;
		tileTransforms[9] = transform9;
		tileTransforms[10] = transform10;
		tileTransforms[11] = transform11;

		// Initialize mirror grid transforms
		mirrorTileTransforms[0] = mirrorTransform0;
		mirrorTileTransforms[1] = mirrorTransform1;
		mirrorTileTransforms[2] = mirrorTransform2;
		mirrorTileTransforms[3] = mirrorTransform3;
		mirrorTileTransforms[4] = mirrorTransform4;
		mirrorTileTransforms[5] = mirrorTransform5;
		mirrorTileTransforms[6] = mirrorTransform6;
		mirrorTileTransforms[7] = mirrorTransform7;
		mirrorTileTransforms[8] = mirrorTransform8;
		mirrorTileTransforms[9] = mirrorTransform9;
		mirrorTileTransforms[10] = mirrorTransform10;
		mirrorTileTransforms[11] = mirrorTransform11;

		// Set all tiles to ALL directions by default (for testing)
		for (auto& row : grid) {
			row.fill(ALL);
		}
		for (auto& row : mirrorGrid) {
			row.fill(ALL);
		}

		// Start both at position (2, 0) - bottom left
		currentRow = 2;
		currentCol = 0;
		mirrorRow = 2;
		mirrorCol = 3;  // Mirror starts at opposite column (3 - 0 = 3)

		// Position the original target
		if (targetTransform.IsValid() && transform8.IsValid()) {
			Vec3 startPos = GetPosition(transform8);  // Tile at [2,0]
			startPos.z += 1.0f; 
			SetPosition(targetTransform, startPos);
			LOG_INFO("Original target placed at (" << currentRow << "," << currentCol << ")");
		}

		// Position the mirror target
		if (mirrorTargetTransform.IsValid() && mirrorTransform11.IsValid()) {
			Vec3 mirrorStartPos = GetPosition(mirrorTransform11);  // Tile at [2,3]
			mirrorStartPos.z += 1.0f;
			SetPosition(mirrorTargetTransform, mirrorStartPos);
			LOG_INFO("Mirror target placed at (" << mirrorRow << "," << mirrorCol << ")");
		}

		LogCurrentState();

		
		//NE::Renderer::Command::AssignMaterial(targetTransform, "41e072ab-c276-4cf3-8b95-6c92401fcdec");
		//NE::Renderer::Command::AssignMaterial(mirrorTargetTransform, "ad9dd997-3747-4fe2-8abe-723a6d7fc27f");
	}

	void Update(double deltaTime) override {
		if (!targetTransform.IsValid() || !mirrorTargetTransform.IsValid()) return;

		// Grid-based movement with WASD
		if (Input::WasKeyPressed('W')) {
			TryMoveUp();
		}
		if (Input::WasKeyPressed('S')) {
			TryMoveDown();
		}
		if (Input::WasKeyPressed('A')) {
			TryMoveLeft();
		}
		if (Input::WasKeyPressed('D')) {
			TryMoveRight();
		}

		// Debug: Press 'P' to print current state
		if (Input::WasKeyPressed('P')) {
			PrintGridState();
		}

		// Check if both reached their goals
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

	const char* GetTypeName() const override {
		return "MatchingPuzzle";
	}

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

	// === MOVEMENT METHODS ===

	void TryMoveUp() {
		LOG_INFO("\n--- Attempting UP ---");

		// Try moving original UP
		bool originalMoved = TryMoveOriginal(UP, -1, 0);

		// Try moving mirror UP (same direction)
		bool mirrorMoved = TryMoveMirror(UP, -1, 0);

		if (originalMoved || mirrorMoved) {
			LogCurrentState();
		}
	}

	void TryMoveDown() {
		LOG_INFO("\n--- Attempting DOWN ---");

		// Try moving original DOWN
		bool originalMoved = TryMoveOriginal(DOWN, 1, 0);

		// Try moving mirror DOWN (same direction)
		bool mirrorMoved = TryMoveMirror(DOWN, 1, 0);

		if (originalMoved || mirrorMoved) {
			LogCurrentState();
		}
	}

	void TryMoveLeft() {
		LOG_INFO("\n--- Attempting LEFT ---");

		// Original moves LEFT
		bool originalMoved = TryMoveOriginal(LEFT, 0, -1);

		// Mirror moves RIGHT (opposite!)
		bool mirrorMoved = TryMoveMirror(RIGHT, 0, 1);

		if (originalMoved || mirrorMoved) {
			LogCurrentState();
		}
	}

	void TryMoveRight() {
		LOG_INFO("\n--- Attempting RIGHT ---");

		// Original moves RIGHT
		bool originalMoved = TryMoveOriginal(RIGHT, 0, 1);

		// Mirror moves LEFT (opposite!)
		bool mirrorMoved = TryMoveMirror(LEFT, 0, -1);

		if (originalMoved || mirrorMoved) {
			LogCurrentState();
		}
	}

	// === HELPER METHODS ===

	bool TryMoveOriginal(Direction dir, int rowDelta, int colDelta) {
		// Check if current tile allows this direction
		if (!CanMoveInDirection(grid[currentRow][currentCol], dir)) {
			LOG_WARNING("Original: Cannot move - current tile doesn't allow it");
			return false;
		}

		// Calculate new position
		int newRow = currentRow + rowDelta;
		int newCol = currentCol + colDelta;

		// Check bounds
		if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
			LOG_WARNING("Original: Cannot move - would go out of bounds");
			return false;
		}

		// Check if destination tile allows entry from opposite direction
		Direction oppositeDir = GetOppositeDirection(dir);
		if (!CanMoveInDirection(grid[newRow][newCol], oppositeDir)) {
			LOG_WARNING("Original: Cannot move - destination doesn't allow entry");
			return false;
		}

		// Move is valid!
		currentRow = newRow;
		currentCol = newCol;
		MoveTargetToTile(targetTransform, tileTransforms[currentRow * 4 + currentCol]);
		LOG_INFO("Original moved to (" << currentRow << "," << currentCol << ")");
		return true;
	}

	bool TryMoveMirror(Direction dir, int rowDelta, int colDelta) {
		// Check if current tile allows this direction
		if (!CanMoveInDirection(mirrorGrid[mirrorRow][mirrorCol], dir)) {
			LOG_WARNING("Mirror: Cannot move - current tile doesn't allow it");
			return false;
		}

		// Calculate new position
		int newRow = mirrorRow + rowDelta;
		int newCol = mirrorCol + colDelta;

		// Check bounds
		if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
			LOG_WARNING("Mirror: Cannot move - would go out of bounds");
			return false;
		}

		// Check if destination tile allows entry from opposite direction
		Direction oppositeDir = GetOppositeDirection(dir);
		if (!CanMoveInDirection(mirrorGrid[newRow][newCol], oppositeDir)) {
			LOG_WARNING("Mirror: Cannot move - destination doesn't allow entry");
			return false;
		}

		// Move is valid!
		mirrorRow = newRow;
		mirrorCol = newCol;
		MoveTargetToTile(mirrorTargetTransform, mirrorTileTransforms[mirrorRow * 4 + mirrorCol]);
		LOG_INFO("Mirror moved to (" << mirrorRow << "," << mirrorCol << ")");
		return true;
	}

	bool CanMoveInDirection(Direction tile, Direction dir) const {
		return (static_cast<uint8_t>(tile) & static_cast<uint8_t>(dir)) != 0;
	}

	Direction GetOppositeDirection(Direction dir) const {
		switch (dir) {
		case UP: return DOWN;
		case DOWN: return UP;
		case LEFT: return RIGHT;
		case RIGHT: return LEFT;
		default: return NONE;
		}
	}

	void MoveTargetToTile(TransformRef& target, TransformRef& tile) {
		if (tile.IsValid() && target.IsValid()) {
			Vec3 tilePos = GetPosition(tile);
			tilePos.z += 1.0f;
			SetPosition(target, tilePos);
		}
	}

	bool HasReachedEnd() const {
		if (!endTransform.IsValid() || !targetTransform.IsValid()) {
			return false;
		}

		Vec3 targetPos = GetPosition(targetTransform);
		Vec3 endPos = GetPosition(endTransform);

		float dx = targetPos.x - endPos.x;
		float dy = targetPos.y - endPos.y;
		float dz = targetPos.z - endPos.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		return distance < 0.5f;
	}

	bool HasMirrorReachedEnd() const {
		if (!mirrorEndTransform.IsValid() || !mirrorTargetTransform.IsValid()) {
			return false;
		}

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

	// === EXPOSED FIELDS ===

	// Original grid
	TransformRef targetTransform;
	TransformRef endTransform;
	TransformRef transform0, transform1, transform2, transform3;
	TransformRef transform4, transform5, transform6, transform7;
	TransformRef transform8, transform9, transform10, transform11;

	// Mirror grid
	TransformRef mirrorTargetTransform;
	TransformRef mirrorEndTransform;
	TransformRef mirrorTransform0, mirrorTransform1, mirrorTransform2, mirrorTransform3;
	TransformRef mirrorTransform4, mirrorTransform5, mirrorTransform6, mirrorTransform7;
	TransformRef mirrorTransform8, mirrorTransform9, mirrorTransform10, mirrorTransform11;

	float tileSpacingX = 1.5f;
	float tileSpacingY = 1.5f;

	// === INTERNAL STATE ===
	std::array<std::array<Direction, 4>, 3> grid;
	std::array<std::array<Direction, 4>, 3> mirrorGrid;

	std::array<TransformRef, 12> tileTransforms;
	std::array<TransformRef, 12> mirrorTileTransforms;

	// Original target position
	int currentRow = 0;
	int currentCol = 0;

	// Mirror target position
	int mirrorRow = 0;
	int mirrorCol = 0;
};