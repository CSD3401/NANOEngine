#pragma once
#include "EngineAPI.hpp"
#include "Misc_ICOSwitcher.hpp"

/*
* Puzzle_Sinkhole
* Toggles present/past parent objects when the stopwatch is activated/deactivated.
* Present objects are hidden when ChronoActivated (past), and shown when ChronoDeactivated (present).
*/

class Misc_Sinkhole : public IScript {
public:
    Misc_Sinkhole() = default;

    ~Misc_Sinkhole() override = default;

    // === Lifecycle Methods ===
    void Awake() override { ValidateICOSwitcher(); }

    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(icoSwitcherRef);
    }
    void Start() override { ValidateICOSwitcher(); }
    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override { ValidateICOSwitcher(); }

    const char* GetTypeName() const override {
        return "Misc_Sinkhole";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    GameObjectRef icoSwitcherRef;
    void ValidateICOSwitcher() {
        if (!icoSwitcherRef.IsValid()) {
            LOG_WARNING("Misc_Sinkhole: missing ICOSwitcher reference");
            return;
        }

        auto* switcher = GameObject(icoSwitcherRef).GetComponent<Misc_ICOSwitcher>();
        if (!switcher) {
            LOG_WARNING("Misc_Sinkhole: ICOSwitcher entity has no Miscellaneous_ICOSwitcher");
        }
    }
};
