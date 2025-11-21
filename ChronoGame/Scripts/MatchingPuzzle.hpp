#pragma once
#include "EngineAPI.hpp"
#include <array>

// GLFW key codes
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_X 88
#define GLFW_KEY_Z 90

class MatchingPuzzle : public IScript {
public:
	MatchingPuzzle() {
		// Component refs for start, end, and all 12 tiles
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

		// Register fields for grid spacing
		//SCRIPT_FIELD(tileSpacingX, Float);
		//SCRIPT_FIELD(tileSpacingY, Float);
	}

	~MatchingPuzzle() override = default;

	void Awake() override {}

	void Initialize(Entity entity) override {}

	void Start() override {
		LOG_INFO("=== MatchingPuzzle Started ===");

		// Initialize the transform array
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

		// Set each tile to ALL directions by default
		for (auto& row : grid) {
			row.fill(ALL);
		}

		// Example: Set up a specific puzzle pattern
		// grid[0][0] = static_cast<Direction>(RIGHT | DOWN);
		// grid[0][1] = static_cast<Direction>(LEFT | RIGHT);
		// grid[1][1] = static_cast<Direction>(UP | DOWN | LEFT | RIGHT);
		// etc.

		// Start at position (0, 0)
		currentRow = 0;
		currentCol = 0;

		// Position the target at the starting tile
		if (targetTransform.IsValid() && transform0.IsValid()) {
			Vec3 startPos = GetPosition(transform0);
			startPos.z += 1;
			SetPosition(targetTransform, startPos);
			LOG_INFO("Target placed at start position (" << currentRow << "," << currentCol << ")");
		}

		LogCurrentTile();
	}

	void Update(double deltaTime) override {
		if (!targetTransform.IsValid()) return;

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

		// Debug: Press 'P' to print current grid state
		if (Input::WasKeyPressed('P')) {
			PrintGridState();
		}

		// Debug: Press 'T' to toggle current tile directions
		if (Input::WasKeyPressed('T')) {
			ToggleCurrentTileDirections();
		}

		// Check if reached end
		if (HasReachedEnd()) {
			static bool hasLogged = false;
			if (!hasLogged) {
				LOG_INFO("PUZZLE SOLVED! Reached the end!");
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
	// === DIRECTION ENUM ===
	enum Direction : uint8_t {
		NONE = 0b0000,  // 0
		UP = 0b0001,    // 1
		RIGHT = 0b0010, // 2
		DOWN = 0b0100,  // 4
		LEFT = 0b1000,  // 8
		ALL = 0b1111    // 15
	};

	// === GRID MOVEMENT METHODS ===

	void TryMoveUp() {
		// Check if current tile allows UP movement
		if (!CanMoveInDirection(UP)) {
			LOG_WARNING("Cannot move UP from current tile!");
			return;
		}

		// Check if we're at the top edge
		if (currentRow == 0) {
			LOG_WARNING("Cannot move UP - at top edge!");
			return;
		}

		// Check if destination tile allows entry from DOWN
		int newRow = currentRow - 1;
		if (!CanEnterFrom(newRow, currentCol, DOWN)) {
			LOG_WARNING("Destination tile doesn't allow entry from below!");
			return;
		}

		// Move is valid!
		currentRow = newRow;
		MoveTargetToCurrentTile();
		LOG_INFO("Moved UP to (" << currentRow << "," << currentCol << ")");
		LogCurrentTile();
	}

	void TryMoveDown() {
		if (!CanMoveInDirection(DOWN)) {
			LOG_WARNING("Cannot move DOWN from current tile!");
			return;
		}

		if (currentRow >= 2) {  // 3 rows (0, 1, 2)
			LOG_WARNING("Cannot move DOWN - at bottom edge!");
			return;
		}

		int newRow = currentRow + 1;
		if (!CanEnterFrom(newRow, currentCol, UP)) {
			LOG_WARNING("Destination tile doesn't allow entry from above!");
			return;
		}

		currentRow = newRow;
		MoveTargetToCurrentTile();
		LOG_INFO("Moved DOWN to (" << currentRow << "," << currentCol << ")");
		LogCurrentTile();
	}

	void TryMoveLeft() {
		if (!CanMoveInDirection(LEFT)) {
			LOG_WARNING("Cannot move LEFT from current tile!");
			return;
		}

		if (currentCol == 0) {
			LOG_WARNING("Cannot move LEFT - at left edge!");
			return;
		}

		int newCol = currentCol - 1;
		if (!CanEnterFrom(currentRow, newCol, RIGHT)) {
			LOG_WARNING("Destination tile doesn't allow entry from right!");
			return;
		}

		currentCol = newCol;
		MoveTargetToCurrentTile();
		LOG_INFO("Moved LEFT to (" << currentRow << "," << currentCol << ")");
		LogCurrentTile();
	}

	void TryMoveRight() {
		if (!CanMoveInDirection(RIGHT)) {
			LOG_WARNING("Cannot move RIGHT from current tile!");
			return;
		}

		if (currentCol >= 3) {  // 4 columns (0, 1, 2, 3)
			LOG_WARNING("Cannot move RIGHT - at right edge!");
			return;
		}

		int newCol = currentCol + 1;
		if (!CanEnterFrom(currentRow, newCol, LEFT)) {
			LOG_WARNING("Destination tile doesn't allow entry from left!");
			return;
		}

		currentCol = newCol;
		MoveTargetToCurrentTile();
		LOG_INFO("Moved RIGHT to (" << currentRow << "," << currentCol << ")");
		LogCurrentTile();
	}

	// === HELPER METHODS ===

	bool CanMoveInDirection(Direction dir) const {
		Direction currentTile = grid[currentRow][currentCol];
		return (static_cast<uint8_t>(currentTile) & static_cast<uint8_t>(dir)) != 0;
	}

	bool CanEnterFrom(int row, int col, Direction fromDirection) const {
		Direction destinationTile = grid[row][col];
		return (static_cast<uint8_t>(destinationTile) & static_cast<uint8_t>(fromDirection)) != 0;
	}

	void MoveTargetToCurrentTile() {
		// Get the transform of the current tile
		int tileIndex = currentRow * 4 + currentCol;  // Convert (row,col) to index
		TransformRef& tileTransform = tileTransforms[tileIndex];

		if (tileTransform.IsValid()) {
			Vec3 tilePos = GetPosition(tileTransform);
			SetPosition(targetTransform, tilePos);
		}
		else {
			LOG_ERROR("Tile transform at index " << tileIndex << " is invalid!");
		}
	}

	bool HasReachedEnd() const {
		if (!endTransform.IsValid() || !targetTransform.IsValid()) {
			return false;
		}

		Vec3 targetPos = GetPosition(targetTransform);
		Vec3 endPos = GetPosition(endTransform);

		// Check if target is close to end (within 0.5 units)
		float dx = targetPos.x - endPos.x;
		float dy = targetPos.y - endPos.y;
		float dz = targetPos.z - endPos.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		return distance < 0.5f;
	}

	void LogCurrentTile() const {
		Direction current = grid[currentRow][currentCol];
		uint8_t val = static_cast<uint8_t>(current);
		LOG_INFO("Current tile (" << currentRow << "," << currentCol << ") allows: "
			<< ((val & static_cast<uint8_t>(UP)) ? "UP " : "")
			<< ((val & static_cast<uint8_t>(DOWN)) ? "DOWN " : "")
			<< ((val & static_cast<uint8_t>(LEFT)) ? "LEFT " : "")
			<< ((val & static_cast<uint8_t>(RIGHT)) ? "RIGHT" : ""));
	}

	void PrintGridState() const {
		LOG_INFO("=== Grid State ===");
		for (int row = 0; row < 3; row++) {
			LOG_INFO("Row " << row << ": "
				<< static_cast<int>(grid[row][0]) << " "
				<< static_cast<int>(grid[row][1]) << " "
				<< static_cast<int>(grid[row][2]) << " "
				<< static_cast<int>(grid[row][3]));
		}
		LOG_INFO("Current position: (" << currentRow << "," << currentCol << ")");
	}

	void ToggleCurrentTileDirections() {
		// Cycle through direction patterns for testing
		Direction& current = grid[currentRow][currentCol];

		if (current == ALL) {
			current = static_cast<Direction>(UP | DOWN);  // Vertical only
		}
		else if (current == static_cast<Direction>(UP | DOWN)) {
			current = static_cast<Direction>(LEFT | RIGHT);  // Horizontal only
		}
		else if (current == static_cast<Direction>(LEFT | RIGHT)) {
			current = NONE;  // Blocked
		}
		else {
			current = ALL;  // Back to all
		}

		LOG_INFO("Toggled tile (" << currentRow << "," << currentCol << ") to: " << static_cast<int>(current));
		LogCurrentTile();
	}

	// === EXPOSED FIELDS ===
	TransformRef targetTransform;  // The moving piece
	TransformRef endTransform;     // The goal position

	// All 12 tile transforms (4 cols x 3 rows)
	TransformRef transform0;   // Row 0, Col 0
	TransformRef transform1;   // Row 0, Col 1
	TransformRef transform2;   // Row 0, Col 2
	TransformRef transform3;   // Row 0, Col 3
	TransformRef transform4;   // Row 1, Col 0
	TransformRef transform5;   // Row 1, Col 1
	TransformRef transform6;   // Row 1, Col 2
	TransformRef transform7;   // Row 1, Col 3
	TransformRef transform8;   // Row 2, Col 0
	TransformRef transform9;   // Row 2, Col 1
	TransformRef transform10;  // Row 2, Col 2
	TransformRef transform11;  // Row 2, Col 3

	//float tileSpacingX = 1.5f;
	//float tileSpacingY = 1.5f;

	// === INTERNAL STATE ===
	std::array<std::array<Direction, 4>, 3> grid;  // 4 cols x 3 rows
	std::array<TransformRef, 12> tileTransforms;    // For easy indexing

	int currentRow = 0;
	int currentCol = 0;
};