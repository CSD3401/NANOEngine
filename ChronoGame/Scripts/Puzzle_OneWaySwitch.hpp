#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"
#include "Puzzle_TwoStateRotater.hpp"

/*
* Puzzle_OneWaySwitch
* One-way switch that can only be activated once, toggling a TwoStateRotater.
*/

class Puzzle_OneWaySwitch : public Interactable_ {
public:
    Puzzle_OneWaySwitch() = default;

    ~Puzzle_OneWaySwitch() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(twoStateRotaterRef);
    }

    void Start() override {
        CacheRotater();
        if (rotater) {
            state = rotater->GetStartingState();
            LOG_INFO("Puzzle_OneWaySwitch: starting state = " << (state ? "true" : "false"));
        } else {
            state = false;
            LOG_WARNING("Puzzle_OneWaySwitch: rotater not set at Start");
        }
    }

    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Puzzle_OneWaySwitch";
    }

    // === Interaction ===
    void Interact() override {
        if (!rotater) {
            CacheRotater();
        }

        if (!rotater) {
            LOG_WARNING("Puzzle_OneWaySwitch: missing Puzzle_TwoStateRotater reference");
            return;
        }

        if (state == false) {
            LOG_INFO("Puzzle_OneWaySwitch: activating switch");
            Interactable_::Interact();
            rotater->SwitchState();
            state = true;
        } else {
            LOG_INFO("Puzzle_OneWaySwitch: already used, ignoring");
        }
    }

    void ResetState() {
        if (!rotater) {
            CacheRotater();
        }

        state = false;
        if (rotater) {
            rotater->SetState(false);
            LOG_INFO("Puzzle_OneWaySwitch: reset state");
        } else {
            LOG_WARNING("Puzzle_OneWaySwitch: reset failed, rotater missing");
        }
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    GameObjectRef twoStateRotaterRef;
    Puzzle_TwoStateRotater* rotater = nullptr;
    bool state = false;

    void CacheRotater() {
        if (!twoStateRotaterRef.IsValid()) {
            rotater = nullptr;
            return;
        }

        rotater = GameObject(twoStateRotaterRef).GetComponent<Puzzle_TwoStateRotater>();
    }
};
