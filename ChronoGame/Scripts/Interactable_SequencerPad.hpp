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

class Interactable_SequencerPad : public Interactable_ {
public:
    Interactable_SequencerPad() = default;
    ~Interactable_SequencerPad() override = default;

    void Initialize(Entity entity) override {
        (void)entity;
        SCRIPT_GAMEOBJECT_REF(sequencer);
        SCRIPT_FIELD(slotNumber, Int);
        SCRIPT_FIELD(startPuzzleIfIdle, Bool);
    }

    void Interact() override {
        if (!sequencer.IsValid()) {
            LOG_WARNING("[SequencerPad] No sequencer assigned (drag the puzzle GameObject into `sequencer`)");
            return;
        }

        GameObject seqGO(sequencer);
        if (!seqGO.IsValid()) {
            LOG_WARNING("[SequencerPad] Sequencer reference is invalid");
            return;
        }

        Puzzle_MultiLightSequencer* puzzle = seqGO.GetComponent<Puzzle_MultiLightSequencer>();
        if (!puzzle) {
            LOG_WARNING("[SequencerPad] Sequencer GameObject has no Puzzle_MultiLightSequencer component");
            return;
        }

        if (startPuzzleIfIdle) {
            puzzle->StartPuzzleIfIdle(true);
        }

        puzzle->ReceiveInput(slotNumber);
    }

    const char* GetTypeName() const override { return "Interactable_SequencerPad"; }

    // Collision Callbacks
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef sequencer{};
    int slotNumber = 1;
    bool startPuzzleIfIdle = false;
};
