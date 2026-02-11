#pragma once

#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include "Puzzle_MultiLightSequencer.hpp"

/*
    Interactable collider-pad for Puzzle_MultiLightSequencer.

    Editor setup:
      1) Create a "pad" GameObject under each light (box/cylinder is fine).
      2) Add a Collider component to the pad.
      3) Put the pad on the same layer that Player_Raycast.targetLayer checks.
      4) Attach this script to the pad.
      5) Assign `sequencer` to the GameObject that has Puzzle_MultiLightSequencer.
      6) Set `slotNumber` to match the light slot (1..9) you want to trigger.

    Notes:
      - This forwards clicks to Puzzle_MultiLightSequencer::ReceiveInput(...)
      - `slotNumber` is 1..9 and maps to light1..light9.
*/

class UI_Notes : public Interactable_ {
public:
    UI_Notes() = default;
    ~UI_Notes() override = default;

    void Initialize(Entity entity) override {
        _SetEntity(entity);

        // Expose in editor
        RegisterGameObjectRefField("Object To Activate", &objectToActivate);
    }

    void Interact() override {
        if (!objectToActivate) return;

        Entity target = objectToActivate.GetEntity();
        bool currentlyActive = IsActive(target);      // <-- whatever your engine calls this
        SetActive(!currentlyActive, target);
    }

    // Collision Callbacks
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef objectToActivate;
};
