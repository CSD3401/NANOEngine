#pragma once

#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include "Puzzle_Mirror.hpp"

/*
    Click-forwarder for MirrorPuzzle tiles.

    Attach this script to each clickable tile collider on either grid.
    When the player raycast clicks the tile, this script forwards the
    tile coordinates back to MirrorPuzzle, which then decides whether the
    click represents a valid one-step adjacent move.
*/
class Interactable_MirrorTile : public Interactable_ {
public:
    Interactable_MirrorTile() = default;
    ~Interactable_MirrorTile() override = default;

    void Initialize(Entity entity) override {
        (void)entity;
        SCRIPT_GAMEOBJECT_REF(puzzle);
        SCRIPT_FIELD(row, Int);
        SCRIPT_FIELD(col, Int);
        SCRIPT_FIELD(isMirrorGrid, Bool);
    }

    void Interact() override {
        if (!puzzle.IsValid()) {
            LOG_WARNING("[MirrorTile] No puzzle assigned");
            return;
        }

        GameObject puzzleGO(puzzle);
        if (!puzzleGO.IsValid()) {
            LOG_WARNING("[MirrorTile] Puzzle GameObject reference is invalid");
            return;
        }

        MirrorPuzzle* mirrorPuzzle = puzzleGO.GetComponent<MirrorPuzzle>();
        if (!mirrorPuzzle) {
            LOG_WARNING("[MirrorTile] Assigned GameObject has no MirrorPuzzle component");
            return;
        }

        mirrorPuzzle->HandleTileClick(row, col, isMirrorGrid);
    }

    const char* GetTypeName() const override { return "Interactable_MirrorTile"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef puzzle{};
    int row = 0;
    int col = 0;
    bool isMirrorGrid = false;
};
