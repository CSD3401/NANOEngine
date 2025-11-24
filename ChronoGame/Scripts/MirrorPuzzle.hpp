#pragma once
#include "EngineAPI.hpp"
#include <array>

/**
 * MirrorPuzzle - Tile-based puzzle with mirrored movement
 *
 * Uses 4 TransformRefs:
 *   - targetTransform, gridParent (original)
 *   - mirrorTargetTransform, mirrorGridParent (mirror)
 *
 * Start/End positions are configured via exposed int fields.
 * Mirror positions are automatically calculated (horizontally mirrored).
 *
 * ============================================================================
 * EXPOSED FIELDS:
 * ============================================================================
 *   - startRow, startCol: Starting tile for original target (0-2, 0-3)
 *   - endRow, endCol: Goal tile for original target (0-2, 0-3)
 *   Mirror positions auto-calculated: mirrorCol = 3 - col
 *
 * ============================================================================
 * HIERARCHY SETUP:
 * ============================================================================
 *
 *   Scene
 *   ├── ScriptHolder (attach this script here)
 *   ├── OriginalGridParent (assign to gridParent)
 *   │   ├── Tile0 (child 0) ... Tile11 (child 11)
 *   ├── MirrorGridParent (assign to mirrorGridParent)
 *   │   ├── Tile0 (child 0) ... Tile11 (child 11)
 *   ├── Target (assign to targetTransform)
 *   └── MirrorTarget (assign to mirrorTargetTransform)
 *
 * Grid Layout (4 columns x 3 rows):
 *   Row 0: [0] [1] [2] [3]
 *   Row 1: [4] [5] [6] [7]
 *   Row 2: [8] [9] [10][11]
 */
class MirrorPuzzle : public IScript {
public:
	// Struct to configure individual tile movement directions
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
		// Component references (use SCRIPT_COMPONENT_REF)
		SCRIPT_COMPONENT_REF(targetTransform, Transform);
		SCRIPT_COMPONENT_REF(gridParent, Transform);
		SCRIPT_COMPONENT_REF(mirrorTargetTransform, Transform);
		SCRIPT_COMPONENT_REF(mirrorGridParent, Transform);

		// Primitive fields (use SCRIPT_FIELD)
		//REGISTER_FIELD(startRow);
		//REGISTER_FIELD(startCol);
		//REGISTER_FIELD(endRow);
		//REGISTER_FIELD(endCol);
		//REGISTER_FIELD(tileSpacingX);
		//REGISTER_FIELD(tileSpacingY);

		SCRIPT_FIELD(startRow, Int);
		SCRIPT_FIELD(startCol, Int);
		SCRIPT_FIELD(endRow, Int);
		SCRIPT_FIELD(endCol, Int);
		SCRIPT_FIELD(tileSpacingX, Float);
		SCRIPT_FIELD(tileSpacingY, Float);

		// Vectors of structs (use REGISTER_VECTOR with NE_REFLECT_BEGIN/END in struct)
		REGISTER_VECTOR(tileRestrictions);
		REGISTER_VECTOR(mirrorTileRestrictions);
	}

	~MirrorPuzzle() override = default;

	void Awake() override {}
	void Initialize(Entity entity) override {}

	void Start() override {
		LOG_INFO("=== MirrorPuzzle Started ===");

		// Validate parent refs
		if (!gridParent.IsValid()) {
			LOG_ERROR("gridParent is not assigned!");
			return;
		}
		if (!mirrorGridParent.IsValid()) {
			LOG_ERROR("mirrorGridParent is not assigned!");
			return;
		}

		// Clamp start/end positions to valid range
		startRow = Clamp(startRow, 0, 2);
		startCol = Clamp(startCol, 0, 3);
		endRow = Clamp(endRow, 0, 2);
		endCol = Clamp(endCol, 0, 3);

		// Calculate mirror positions (horizontally mirrored)
		mirrorStartRow = startRow;
		mirrorStartCol = 3 - startCol;
		mirrorEndRow = endRow;
		mirrorEndCol = 3 - endCol;

		LOG_INFO("Original: Start(" << startRow << "," << startCol << ") -> End(" << endRow << "," << endCol << ")");
		LOG_INFO("Mirror:   Start(" << mirrorStartRow << "," << mirrorStartCol << ") -> End(" << mirrorEndRow << "," << mirrorEndCol << ")");

		// Cache original grid tile transforms
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

		// Cache mirror grid tile transforms
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

		// Initialize direction grids - all tiles allow all directions by default
		for (auto& row : grid) {
			row.fill(ALL);
		}
		for (auto& row : mirrorGrid) {
			row.fill(ALL);
		}

		// Apply user-configured tile restrictions for original grid
		for (const auto& restriction : tileRestrictions) {
			if (restriction.row >= 0 && restriction.row < 3 && restriction.col >= 0 && restriction.col < 4) {
				Direction allowed = NONE;
				if (restriction.allowUp) allowed = static_cast<Direction>(allowed | UP);
				if (restriction.allowDown) allowed = static_cast<Direction>(allowed | DOWN);
				if (restriction.allowLeft) allowed = static_cast<Direction>(allowed | LEFT);
				if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

				grid[restriction.row][restriction.col] = allowed;
				LOG_INFO("Original tile (" << restriction.row << "," << restriction.col << ") restricted to: " << static_cast<int>(allowed));
			}
		}

		// Apply user-configured tile restrictions for mirror grid
		for (const auto& restriction : mirrorTileRestrictions) {
			if (restriction.row >= 0 && restriction.row < 3 && restriction.col >= 0 && restriction.col < 4) {
				Direction allowed = NONE;
				if (restriction.allowUp) allowed = static_cast<Direction>(allowed | UP);
				if (restriction.allowDown) allowed = static_cast<Direction>(allowed | DOWN);
				if (restriction.allowLeft) allowed = static_cast<Direction>(allowed | LEFT);
				if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

				mirrorGrid[restriction.row][restriction.col] = allowed;
				LOG_INFO("Mirror tile (" << restriction.row << "," << restriction.col << ") restricted to: " << static_cast<int>(allowed));
			}
		}

		// Set current positions to start positions
		currentRow = startRow;
		currentCol = startCol;
		mirrorRow = mirrorStartRow;
		mirrorCol = mirrorStartCol;

		// Position original target on starting tile
		if (targetTransform.IsValid()) {
			int startTileIndex = startRow * 4 + startCol;
			if (tileTransforms[startTileIndex].IsValid()) {
				Vec3 startPos = GetTileWorldPosition(tileTransforms[startTileIndex], gridParent);
				startPos.z += 1.0f;
				SetPosition(targetTransform, startPos);
				LOG_INFO("Original target placed at (" << currentRow << "," << currentCol << ")");
			}
		}

		// Position mirror target on starting tile
		if (mirrorTargetTransform.IsValid()) {
			int mirrorStartTileIndex = mirrorStartRow * 4 + mirrorStartCol;
			if (mirrorTileTransforms[mirrorStartTileIndex].IsValid()) {
				Vec3 mirrorStartPos = GetTileWorldPosition(mirrorTileTransforms[mirrorStartTileIndex], mirrorGridParent);
				mirrorStartPos.z += 1.0f;
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
		if (Input::WasKeyPressed('P')) PrintGridState();
		if (Input::WasKeyPressed('R')) ResetPuzzle();

		// Check win condition
		if (HasReachedEnd() && HasMirrorReachedEnd()) {
			puzzleSolved = true;
			LOG_INFO("*** PUZZLE SOLVED! Both targets reached their goals! ***");
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

	// === Exposed editable fields ===
	// Merge fields from both IScript's built-in system (component refs) and ExposedFieldRegistry (primitives/vectors)
	std::vector<std::string> GetExposedFieldNames() const override { 
		// Get built-in SDK fields (component references)
		auto sdkFields = IScript::GetExposedFieldNames();
		
		// Get ExposedFieldRegistry fields (primitives, vectors)
		auto customFields = m_fields.GetNames();
		
		// Merge both lists
		std::vector<std::string> allFields;
		allFields.reserve(sdkFields.size() + customFields.size());
		allFields.insert(allFields.end(), sdkFields.begin(), sdkFields.end());
		allFields.insert(allFields.end(), customFields.begin(), customFields.end());
		
		return allFields;
	}
	
	std::string GetFieldType(const std::string& name) const override { 
		// Check ExposedFieldRegistry first
		std::string type = m_fields.GetType(name);
		if (!type.empty()) return type;
		
		// Fall back to SDK built-in system
		return IScript::GetFieldType(name);
	}
	
	std::string GetFieldValueAsString(const std::string& name) const override { 
		// Check ExposedFieldRegistry first
		std::string value = m_fields.GetValue(name);
		if (!value.empty()) return value;
		
		// Fall back to SDK built-in system
		return IScript::GetFieldValueAsString(name);
	}
	
	bool SetFieldValueFromString(const std::string& name, const std::string& value) override { 
		// Try ExposedFieldRegistry first
		if (m_fields.SetValue(name, value)) return true;
		
		// Fall back to SDK built-in system
		return IScript::SetFieldValueFromString(name, value);
	}

	// Array/Vector support for tile restrictions
	size_t GetArraySize(const std::string& fieldName) const override {
		return m_fields.GetArraySize(fieldName);
	}

	std::string GetArrayElement(const std::string& fieldName, size_t index) const override {
		return m_fields.GetArrayElement(fieldName, index);
	}

	bool SetArrayElement(const std::string& fieldName, size_t index, const std::string& value) override {
		return m_fields.SetArrayElement(fieldName, index, value);
	}

	void AddArrayElement(const std::string& fieldName) override {
		m_fields.AddArrayElement(fieldName);
	}

	void RemoveArrayElement(const std::string& fieldName, size_t index) override {
		m_fields.RemoveArrayElement(fieldName, index);
	}

private:
	enum Direction : uint8_t {
		NONE = 0b0000,
		UP = 0b0001,
		RIGHT = 0b0010,
		DOWN = 0b0100,
		LEFT = 0b1000,
		ALL = 0b1111
	};

	// === UTILITY ===

	int Clamp(int value, int minVal, int maxVal) const {
		if (value < minVal) return minVal;
		if (value > maxVal) return maxVal;
		return value;
	}

	Vec3 GetTileWorldPosition(TransformRef& tile, TransformRef& parent) {
		Vec3 localPos = GetPosition(tile);
		Vec3 parentPos = GetPosition(parent);
		return Vec3(localPos.x + parentPos.x, localPos.y + parentPos.y, localPos.z + parentPos.z);
	}

	// === MOVEMENT ===

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
		bool mirrorMoved = TryMoveMirror(RIGHT, 0, 1);  // Mirror moves opposite horizontally
		if (originalMoved || mirrorMoved) LogCurrentState();
	}

	void TryMoveRight() {
		LOG_INFO("\n--- Attempting RIGHT ---");
		bool originalMoved = TryMoveOriginal(RIGHT, 0, 1);
		bool mirrorMoved = TryMoveMirror(LEFT, 0, -1);  // Mirror moves opposite horizontally
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

	// === WIN CONDITION (now based on grid coordinates) ===

	bool HasReachedEnd() const {
		return (currentRow == endRow && currentCol == endCol);
	}

	bool HasMirrorReachedEnd() const {
		return (mirrorRow == mirrorEndRow && mirrorCol == mirrorEndCol);
	}

	// === RESET ===

	void ResetPuzzle() {
		LOG_INFO("\n=== Resetting Puzzle ===");

		currentRow = startRow;
		currentCol = startCol;
		mirrorRow = mirrorStartRow;
		mirrorCol = mirrorStartCol;
		puzzleSolved = false;

		// Reposition targets
		if (targetTransform.IsValid()) {
			int startTileIndex = startRow * 4 + startCol;
			if (tileTransforms[startTileIndex].IsValid()) {
				Vec3 startPos = GetTileWorldPosition(tileTransforms[startTileIndex], gridParent);
				startPos.z += 1.0f;
				SetPosition(targetTransform, startPos);
			}
		}

		if (mirrorTargetTransform.IsValid()) {
			int mirrorStartTileIndex = mirrorStartRow * 4 + mirrorStartCol;
			if (mirrorTileTransforms[mirrorStartTileIndex].IsValid()) {
				Vec3 mirrorStartPos = GetTileWorldPosition(mirrorTileTransforms[mirrorStartTileIndex], mirrorGridParent);
				mirrorStartPos.z += 1.0f;
				SetPosition(mirrorTargetTransform, mirrorStartPos);
			}
		}

		LogCurrentState();
	}

	// === DEBUG ===

	void LogCurrentState() const {
		LOG_INFO("=== Current State ===");
		LOG_INFO("Original: (" << currentRow << "," << currentCol << ") -> Goal(" << endRow << "," << endCol << ")");
		LOG_INFO("Mirror:   (" << mirrorRow << "," << mirrorCol << ") -> Goal(" << mirrorEndRow << "," << mirrorEndCol << ")");
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
		LOG_INFO("Original goal: (" << endRow << "," << endCol << ")");

		LOG_INFO("\nMIRROR GRID:");
		for (int row = 0; row < 3; row++) {
			LOG_INFO("Row " << row << ": "
				<< static_cast<int>(mirrorGrid[row][0]) << " "
				<< static_cast<int>(mirrorGrid[row][1]) << " "
				<< static_cast<int>(mirrorGrid[row][2]) << " "
				<< static_cast<int>(mirrorGrid[row][3]));
		}
		LOG_INFO("Mirror position: (" << mirrorRow << "," << mirrorCol << ")");
		LOG_INFO("Mirror goal: (" << mirrorEndRow << "," << mirrorEndCol << ")");
	}

	// === EXPOSED FIELDS (4 refs + 4 position ints) ===

	TransformRef targetTransform;
	TransformRef gridParent;
	TransformRef mirrorTargetTransform;
	TransformRef mirrorGridParent;

	// User-configurable start/end positions (exposed in editor)
	int startRow = 2;  // Default: bottom-left
	int startCol = 0;
	int endRow = 0;    // Default: top-right
	int endCol = 3;

	float tileSpacingX = 1.5f;
	float tileSpacingY = 1.5f;

	// Tile movement restrictions (configurable in editor)
	std::vector<TileConfig> tileRestrictions;        // Original grid restrictions
	std::vector<TileConfig> mirrorTileRestrictions;  // Mirror grid restrictions

	// === INTERNAL STATE ===

	// Mirror positions (calculated from original)
	int mirrorStartRow = 0;
	int mirrorStartCol = 0;
	int mirrorEndRow = 0;
	int mirrorEndCol = 0;

	// Current positions
	int currentRow = 0;
	int currentCol = 0;
	int mirrorRow = 0;
	int mirrorCol = 0;

	bool puzzleSolved = false;

	// Direction grids
	std::array<std::array<Direction, 4>, 3> grid;
	std::array<std::array<Direction, 4>, 3> mirrorGrid;

	// Tile transforms (populated from GetChildOf in Start)
	std::array<TransformRef, 12> tileTransforms;
	std::array<TransformRef, 12> mirrorTileTransforms;

	// Field registry for editor integration
	ExposedFieldRegistry m_fields;
};