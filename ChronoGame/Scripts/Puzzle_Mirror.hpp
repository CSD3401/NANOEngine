#pragma once
#include "EngineAPI.hpp"
#include <array>

/**
 * MirrorPuzzle - Dual-grid puzzle with mirrored movement (3x4 grid)
 *
 * This is a complete mirror puzzle where:
 * - Left grid: Main grid (3 rows x 4 columns)
 * - Right grid: Mirror grid (3 rows x 4 columns)
 * - Movement on left causes mirrored X-axis movement on right
 * - Both grids must have valid paths for movement to occur
 *
 * Controls:
 * - WASD: Move navigator
 * - Movement mirrors: LEFT<->RIGHT, UP=UP, DOWN=DOWN
 *
 * Setup in editor:
 * 1. Assign all 12 original tiles to tile00, tile01, ... tile23 (row, col)
 * 2. Assign all 12 mirror tiles to mirrorTile00, ... mirrorTile23
 * 3. Assign targetTransform (navigator on original grid)
 * 4. Assign mirrorTargetTransform (navigator on mirror grid)
 * 5. Configure tileRestrictions and mirrorTileRestrictions vectors
 */
class MirrorPuzzle : public IScript {
public:
    /**
     * Direction flags for movement validation
     */
    enum Direction : uint8_t {
        NONE = 0,
        UP = 1 << 0,
        DOWN = 1 << 1,
        LEFT = 1 << 2,
        RIGHT = 1 << 3,
        ALL = UP | DOWN | LEFT | RIGHT
    };

    MirrorPuzzle() {

        SCRIPT_GAMEOBJECT_REF(mazeServerDoor);

        // Navigator entities
        SCRIPT_COMPONENT_REF(targetTransform, TransformRef);
        SCRIPT_COMPONENT_REF(mirrorTargetTransform, TransformRef);

        // Original grid tiles (3x4 grid = 12 tiles)
        // Row 0
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

        // Mirror grid tiles (3x4 grid = 12 tiles)
        // Row 0
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

    void Initialize(Entity entity) override {
        // Start and end positions
        SCRIPT_FIELD(startRow, Int);
        SCRIPT_FIELD(startCol, Int);
        SCRIPT_FIELD(endRow, Int);
        SCRIPT_FIELD(endCol, Int);

        // Visual offset for navigators
        SCRIPT_FIELD(zOffset, Float);

        // Event to send when solved
        SCRIPT_FIELD(eventName, String);

        // Debug
        SCRIPT_FIELD(debugMode, Bool);
    }

    void Awake() override {
        LOG_DEBUG("MirrorPuzzle Awake");
    }

    void Start() override {
        LOG_DEBUG("=== MirrorPuzzle Started ===");

        // Clamp positions to valid range
        startRow = Clamp(startRow, 0, 2);
        startCol = Clamp(startCol, 0, 3);
        endRow = Clamp(endRow, 0, 2);
        endCol = Clamp(endCol, 0, 3);

        // Calculate mirrored positions (X-axis mirror)
        mirrorStartRow = startRow;
        mirrorStartCol = 3 - startCol;  // Mirror column
        mirrorEndRow = endRow;
        mirrorEndCol = 3 - endCol;      // Mirror column

        LOG_DEBUG("Original: Start({}, {}) -> End({}, {})", startRow, startCol, endRow, endCol);
        LOG_DEBUG("Mirror:   Start({}, {}) -> End({}, {})", mirrorStartRow, mirrorStartCol, mirrorEndRow, mirrorEndCol);

        // Cache tile transforms into arrays for easy access
        CacheTileReferences();

        // Apply tile restrictions (clears all tiles first, then sets specific restrictions)
        ApplyTileRestrictions();

        // Set initial navigator positions
        currentRow = startRow;
        currentCol = startCol;
        mirrorRow = mirrorStartRow;
        mirrorCol = mirrorStartCol;

        // Position navigators at start tiles
        PositionNavigators();

        puzzleSolved = false;
        LogCurrentState();
    }

    void Update(double deltaTime) override {
        if (puzzleSolved) return;
        if (!targetTransform.IsValid() || !mirrorTargetTransform.IsValid()) {
            LOG_DEBUG("ERROR: Navigator transforms not assigned!");
            return;
        }

        // Handle WASD input (commented out for now)
        /*
        if (Input::WasKeyPressed('W') || Input::WasKeyPressed(VK_UP)) {
            TryMoveUp();
        }
        if (Input::WasKeyPressed('S') || Input::WasKeyPressed(VK_DOWN)) {
            TryMoveDown();
        }
        if (Input::WasKeyPressed('A') || Input::WasKeyPressed(VK_LEFT)) {
            TryMoveLeft();
        }
        if (Input::WasKeyPressed('D') || Input::WasKeyPressed(VK_RIGHT)) {
            TryMoveRight();
        }
        */

        // Check win condition
        if (HasReachedEnd() && HasMirrorReachedEnd()) {
            if (!puzzleSolved) {
                puzzleSolved = true;
                LOG_DEBUG("=== PUZZLE SOLVED! ===");

                // Send event
                if (!eventName.empty()) {
                    Events::Send(eventName.c_str(), nullptr);
                }

                SetActive(true, mazeServerDoor.GetEntity());
            }
        }

        // Debug: Reset puzzle
        if (debugMode && Input::WasKeyPressed('R')) {
            ResetPuzzle();
        }
    }

    const char* GetTypeName() const override {
        return "MirrorPuzzle";
    }

    // Required collision/trigger callbacks (not used but must be implemented)
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    // ========== Helper Functions ==========

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

    /**
     * Cache all tile references into arrays for indexed access
     */
    void CacheTileReferences() {
        // Original grid
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

        // Mirror grid
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
    }

    /**
     * Clear all tile restrictions (set all to NONE - no movement)
     */
    void ClearAllRestrictions() {
        // Set all tiles on both grids to NONE (no movement allowed)
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 4; col++) {
                grid[row][col] = NONE;
                mirrorGrid[row][col] = NONE;

                // Update visual indicators to hide all arrows
                UpdateTileIndicators(row, col, NONE, false);  // Left grid
                UpdateTileIndicators(row, col, NONE, true);   // Right grid
            }
        }
        LOG_DEBUG("All tile restrictions cleared (set to NONE) and visual indicators updated");
    }

    /**
     * Apply tile restrictions from configuration
     */
    void ApplyTileRestrictions() {
        // First, clear all tiles (set everything to NONE)
        ClearAllRestrictions();

        // Hardcoded puzzle configuration
        // You can change this directly in code to design different puzzles

        // ORIGINAL GRID - Simple corridor puzzle
        SetTileRestriction(2, 0, false, false, false, true);   // Bottom-left: can only go right
        SetTileRestriction(2, 1, false, false, true, true);    // Can go left or right
        SetTileRestriction(2, 2, false, false, true, true);    // Can go left or right
        SetTileRestriction(2, 3, true, false, true, false);    // Can go up or left
        SetTileRestriction(1, 3, true, true, false, false);    // Can go up or down
        SetTileRestriction(0, 3, false, true, false, false);   // Top-right goal: can only enter from below

        // MIRROR GRID - Mirrored version (LEFT/RIGHT flipped)
        SetMirrorTileRestriction(2, 3, false, false, true, false);   // Bottom-right: can only go left
        SetMirrorTileRestriction(2, 2, false, false, true, true);    // Can go left or right
        SetMirrorTileRestriction(2, 1, false, false, true, true);    // Can go left or right
        SetMirrorTileRestriction(2, 0, true, false, false, true);    // Can go up or right
        SetMirrorTileRestriction(1, 0, true, true, false, false);    // Can go up or down
        SetMirrorTileRestriction(0, 0, false, true, false, false);   // Top-left goal: can only enter from below

        LOG_DEBUG("Tile restrictions applied (hardcoded)");
    }

    /**
     * Helper: Set restriction for original grid tile
     */
    void SetTileRestriction(int row, int col, bool up, bool down, bool left, bool right) {
        if (row >= 0 && row < 3 && col >= 0 && col < 4) {
            Direction allowed = NONE;
            if (up)    allowed = static_cast<Direction>(allowed | UP);
            if (down)  allowed = static_cast<Direction>(allowed | DOWN);
            if (left)  allowed = static_cast<Direction>(allowed | LEFT);
            if (right) allowed = static_cast<Direction>(allowed | RIGHT);

            grid[row][col] = allowed;
            LOG_DEBUG("Original tile ({}, {}) restricted to: {}", row, col, static_cast<int>(allowed));

            UpdateTileIndicators(row, col, allowed, false);
        }
    }

    /**
     * Helper: Set restriction for mirror grid tile
     */
    void SetMirrorTileRestriction(int row, int col, bool up, bool down, bool left, bool right) {
        if (row >= 0 && row < 3 && col >= 0 && col < 4) {
            Direction allowed = NONE;
            if (up)    allowed = static_cast<Direction>(allowed | UP);
            if (down)  allowed = static_cast<Direction>(allowed | DOWN);
            if (left)  allowed = static_cast<Direction>(allowed | LEFT);
            if (right) allowed = static_cast<Direction>(allowed | RIGHT);

            mirrorGrid[row][col] = allowed;
            LOG_DEBUG("Mirror tile ({}, {}) restricted to: {}", row, col, static_cast<int>(allowed));

            UpdateTileIndicators(row, col, allowed, true);
        }
    }

    /**
     * Apply tile restrictions from configuration
     * OLD METHOD - Not used anymore since we don't have vector of struct
     */
    void ApplyTileRestrictionsOLD() {
        // OLD CODE - Not used anymore since we don't have vector of struct
        /*
        // Apply restrictions to original grid
        for (const auto& restriction : tileRestrictions) {
            if (restriction.row >= 0 && restriction.row < 3 &&
                restriction.col >= 0 && restriction.col < 4) {

                Direction allowed = NONE;
                if (restriction.allowUp)    allowed = static_cast<Direction>(allowed | UP);
                if (restriction.allowDown)  allowed = static_cast<Direction>(allowed | DOWN);
                if (restriction.allowLeft)  allowed = static_cast<Direction>(allowed | LEFT);
                if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

                grid[restriction.row][restriction.col] = allowed;
                LOG_DEBUG("Original tile ({}, {}) restricted to: {}",
                    restriction.row, restriction.col, static_cast<int>(allowed));

                UpdateTileIndicators(restriction.row, restriction.col, allowed, false);
            }
        }

        // Apply restrictions to mirror grid
        for (const auto& restriction : mirrorTileRestrictions) {
            if (restriction.row >= 0 && restriction.row < 3 &&
                restriction.col >= 0 && restriction.col < 4) {

                Direction allowed = NONE;
                if (restriction.allowUp)    allowed = static_cast<Direction>(allowed | UP);
                if (restriction.allowDown)  allowed = static_cast<Direction>(allowed | DOWN);
                if (restriction.allowLeft)  allowed = static_cast<Direction>(allowed | LEFT);
                if (restriction.allowRight) allowed = static_cast<Direction>(allowed | RIGHT);

                mirrorGrid[restriction.row][restriction.col] = allowed;
                LOG_DEBUG("Mirror tile ({}, {}) restricted to: {}",
                    restriction.row, restriction.col, static_cast<int>(allowed));

                UpdateTileIndicators(restriction.row, restriction.col, allowed, true);
            }
        }
        */
    }

    /**
     * Update visual direction indicators on a tile
     * Child order: 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT
     */
    void UpdateTileIndicators(int row, int col, Direction allowed, bool isMirror) {
        int tileIndex = row * 4 + col;
        const TransformRef& tileRef = isMirror ? mirrorTileTransforms[tileIndex] : tileTransforms[tileIndex];

        if (!tileRef.IsValid()) return;

        Entity tileEntity = tileRef.GetEntity();

        // Check if tile has children (arrow indicators)
        size_t childCount = GetChildCount(tileEntity);
        if (childCount < 4) {
            if (debugMode) {
                LOG_DEBUG("Tile ({}, {}) has {} children, need 4 for arrows", row, col, childCount);
            }
            return;
        }

        // Get arrow indicator children (0=UP, 1=RIGHT, 2=DOWN, 3=LEFT)
        Entity upIndicator = GetChild(0, tileEntity);
        Entity rightIndicator = GetChild(1, tileEntity);
        Entity downIndicator = GetChild(2, tileEntity);
        Entity leftIndicator = GetChild(3, tileEntity);

        // Set active state based on allowed directions
        if (upIndicator != 0)    SetActive((allowed & UP) != 0, upIndicator);
        if (rightIndicator != 0) SetActive((allowed & RIGHT) != 0, rightIndicator);
        if (downIndicator != 0)  SetActive((allowed & DOWN) != 0, downIndicator);
        if (leftIndicator != 0)  SetActive((allowed & LEFT) != 0, leftIndicator);

        if (debugMode) {
            LOG_DEBUG("Updated indicators for {} tile ({}, {}): UP={}, RIGHT={}, DOWN={}, LEFT={}",
                isMirror ? "mirror" : "original", row, col,
                (allowed & UP) != 0, (allowed & RIGHT) != 0,
                (allowed & DOWN) != 0, (allowed & LEFT) != 0);
        }
    }

    /**
     * Position navigators at their current grid positions
     */
    void PositionNavigators() {
        // Position original navigator
        if (targetTransform.IsValid()) {
            int startTileIndex = currentRow * 4 + currentCol;
            if (tileTransforms[startTileIndex].IsValid()) {
                Vec3 startPos = GetTileWorldPosition(tileTransforms[startTileIndex]);
                startPos.z += zOffset;
                SetPosition(targetTransform, startPos);
                LOG_DEBUG("Original navigator placed at grid ({}, {}), world pos ({}, {}, {})",
                    currentRow, currentCol, startPos.x, startPos.y, startPos.z);
            }
        }

        // Position mirror navigator
        if (mirrorTargetTransform.IsValid()) {
            int mirrorTileIndex = mirrorRow * 4 + mirrorCol;
            if (mirrorTileTransforms[mirrorTileIndex].IsValid()) {
                Vec3 mirrorPos = GetTileWorldPosition(mirrorTileTransforms[mirrorTileIndex]);
                mirrorPos.z += zOffset;
                SetPosition(mirrorTargetTransform, mirrorPos);
                LOG_DEBUG("Mirror navigator placed at grid ({}, {}), world pos ({}, {}, {})",
                    mirrorRow, mirrorCol, mirrorPos.x, mirrorPos.y, mirrorPos.z);
            }
        }
    }

    // ========== Movement Functions ==========

    void TryMoveUp() {
        if (debugMode) LOG_DEBUG("--- Attempting UP ---");

        bool originalMoved = TryMoveOriginal(UP, -1, 0);
        bool mirrorMoved = TryMoveMirror(UP, -1, 0);

        if (originalMoved || mirrorMoved) {
            LogCurrentState();
        }
    }

    void TryMoveDown() {
        if (debugMode) LOG_DEBUG("--- Attempting DOWN ---");

        bool originalMoved = TryMoveOriginal(DOWN, 1, 0);
        bool mirrorMoved = TryMoveMirror(DOWN, 1, 0);

        if (originalMoved || mirrorMoved) {
            LogCurrentState();
        }
    }

    void TryMoveLeft() {
        if (debugMode) LOG_DEBUG("--- Attempting LEFT ---");

        // Left on original = Right on mirror (X-axis flip)
        bool originalMoved = TryMoveOriginal(LEFT, 0, -1);
        bool mirrorMoved = TryMoveMirror(RIGHT, 0, 1);

        if (originalMoved || mirrorMoved) {
            LogCurrentState();
        }
    }

    void TryMoveRight() {
        if (debugMode) LOG_DEBUG("--- Attempting RIGHT ---");

        // Right on original = Left on mirror (X-axis flip)
        bool originalMoved = TryMoveOriginal(RIGHT, 0, 1);
        bool mirrorMoved = TryMoveMirror(LEFT, 0, -1);

        if (originalMoved || mirrorMoved) {
            LogCurrentState();
        }
    }

    /**
     * Try to move on the original grid
     */
    bool TryMoveOriginal(Direction dir, int rowDelta, int colDelta) {
        // Check if current tile allows movement in this direction
        if (!CanMoveInDirection(grid[currentRow][currentCol], dir)) {
            if (debugMode) LOG_DEBUG("Original: Cannot move - current tile doesn't allow it");
            return false;
        }

        // Calculate new position
        int newRow = currentRow + rowDelta;
        int newCol = currentCol + colDelta;

        // Check bounds
        if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
            if (debugMode) LOG_DEBUG("Original: Cannot move - would go out of bounds");
            return false;
        }

        // Check if destination tile allows entry from opposite direction
        Direction oppositeDir = GetOppositeDirection(dir);
        if (!CanMoveInDirection(grid[newRow][newCol], oppositeDir)) {
            if (debugMode) LOG_DEBUG("Original: Cannot move - destination doesn't allow entry");
            return false;
        }

        // Valid move!
        currentRow = newRow;
        currentCol = newCol;
        MoveOriginalTargetToTile(currentRow * 4 + currentCol);

        if (debugMode) LOG_DEBUG("Original moved to ({}, {})", currentRow, currentCol);
        return true;
    }

    /**
     * Try to move on the mirror grid
     */
    bool TryMoveMirror(Direction dir, int rowDelta, int colDelta) {
        // Check if current tile allows movement in this direction
        if (!CanMoveInDirection(mirrorGrid[mirrorRow][mirrorCol], dir)) {
            if (debugMode) LOG_DEBUG("Mirror: Cannot move - current tile doesn't allow it");
            return false;
        }

        // Calculate new position
        int newRow = mirrorRow + rowDelta;
        int newCol = mirrorCol + colDelta;

        // Check bounds
        if (newRow < 0 || newRow > 2 || newCol < 0 || newCol > 3) {
            if (debugMode) LOG_DEBUG("Mirror: Cannot move - would go out of bounds");
            return false;
        }

        // Check if destination tile allows entry from opposite direction
        Direction oppositeDir = GetOppositeDirection(dir);
        if (!CanMoveInDirection(mirrorGrid[newRow][newCol], oppositeDir)) {
            if (debugMode) LOG_DEBUG("Mirror: Cannot move - destination doesn't allow entry");
            return false;
        }

        // Valid move!
        mirrorRow = newRow;
        mirrorCol = newCol;
        MoveMirrorTargetToTile(mirrorRow * 4 + mirrorCol);

        if (debugMode) LOG_DEBUG("Mirror moved to ({}, {})", mirrorRow, mirrorCol);
        return true;
    }

    /**
     * Check if a tile allows movement in a specific direction
     */
    bool CanMoveInDirection(Direction tile, Direction dir) const {
        return (static_cast<uint8_t>(tile) & static_cast<uint8_t>(dir)) != 0;
    }

    /**
     * Get the opposite direction
     */
    Direction GetOppositeDirection(Direction dir) const {
        switch (dir) {
        case UP:    return DOWN;
        case DOWN:  return UP;
        case LEFT:  return RIGHT;
        case RIGHT: return LEFT;
        default:    return NONE;
        }
    }

    /**
     * Move original navigator to a specific tile
     */
    void MoveOriginalTargetToTile(int tileIndex) {
        if (tileTransforms[tileIndex].IsValid() && targetTransform.IsValid()) {
            Vec3 tilePos = GetTileWorldPosition(tileTransforms[tileIndex]);
            tilePos.z += zOffset;
            SetPosition(targetTransform, tilePos);
        }
    }

    /**
     * Move mirror navigator to a specific tile
     */
    void MoveMirrorTargetToTile(int tileIndex) {
        if (mirrorTileTransforms[tileIndex].IsValid() && mirrorTargetTransform.IsValid()) {
            Vec3 tilePos = GetTileWorldPosition(mirrorTileTransforms[tileIndex]);
            tilePos.z += zOffset;
            SetPosition(mirrorTargetTransform, tilePos);
        }
    }

    /**
     * Check if original navigator reached the goal
     */
    bool HasReachedEnd() const {
        return (currentRow == endRow && currentCol == endCol);
    }

    /**
     * Check if mirror navigator reached the goal
     */
    bool HasMirrorReachedEnd() const {
        return (mirrorRow == mirrorEndRow && mirrorCol == mirrorEndCol);
    }

    /**
     * Reset puzzle to start state
     */
    void ResetPuzzle() {
        currentRow = startRow;
        currentCol = startCol;
        mirrorRow = mirrorStartRow;
        mirrorCol = mirrorStartCol;
        puzzleSolved = false;

        PositionNavigators();
        LogCurrentState();

        LOG_DEBUG("Puzzle reset!");
    }

    /**
     * Log current state for debugging
     */
    void LogCurrentState() const {
        LOG_DEBUG("=== Current State ===");
        LOG_DEBUG("Original: ({}, {}) -> Goal({}, {})", currentRow, currentCol, endRow, endCol);
        LOG_DEBUG("Mirror:   ({}, {}) -> Goal({}, {})", mirrorRow, mirrorCol, mirrorEndRow, mirrorEndCol);
    }

    // ========== Fields ==========

    // Door to Unlock
	GameObjectRef mazeServerDoor;

    // Navigator transforms
    TransformRef targetTransform;
    TransformRef mirrorTargetTransform;

    // Original grid tiles (3 rows x 4 cols)
    TransformRef tile00, tile01, tile02, tile03;
    TransformRef tile10, tile11, tile12, tile13;
    TransformRef tile20, tile21, tile22, tile23;

    // Mirror grid tiles (3 rows x 4 cols)
    TransformRef mirrorTile00, mirrorTile01, mirrorTile02, mirrorTile03;
    TransformRef mirrorTile10, mirrorTile11, mirrorTile12, mirrorTile13;
    TransformRef mirrorTile20, mirrorTile21, mirrorTile22, mirrorTile23;

    // Configuration
    int startRow = 2;
    int startCol = 0;
    int endRow = 0;
    int endCol = 3;
    float zOffset = 0.2f;
    std::string eventName = "MirrorPuzzleSolved";
    bool debugMode = true;

    // Runtime state
    int mirrorStartRow = 0;
    int mirrorStartCol = 0;
    int mirrorEndRow = 0;
    int mirrorEndCol = 0;
    int currentRow = 0;
    int currentCol = 0;
    int mirrorRow = 0;
    int mirrorCol = 0;
    bool puzzleSolved = false;

    // Grid data (3 rows x 4 cols)
    std::array<std::array<Direction, 4>, 3> grid;
    std::array<std::array<Direction, 4>, 3> mirrorGrid;

    // Cached tile references for indexed access
    std::array<TransformRef, 12> tileTransforms;
    std::array<TransformRef, 12> mirrorTileTransforms;
};