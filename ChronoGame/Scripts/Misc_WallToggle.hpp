#pragma once
#include "EngineAPI.hpp"

/**
 * Misc_WallToggle
 *
 * Controls wall/barrier colliders across time states.
 * Listens to ChronoActivated/ChronoDeactivated and toggles walls.
 *
 * Setup:
 * 1. Add this script to any manager entity
 * 2. Assign wallObjects (list of GameObjects with colliders)
 *    - If wallObjects is empty, wallObject will be used as a single fallback
 * 3. Walls will be disabled when ChronoActivated, enabled when ChronoDeactivated
 */
class Misc_WallToggle : public IScript {
public:
    Misc_WallToggle() {
    }

    ~Misc_WallToggle() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        RegisterGameObjectRefVectorField("wallObjects", &wallObjects);
    }

    void Start() override {
        Events::Listen("ChronoActivated", [this](void* data) {
            (void)data;
            DisableWalls();
        });

        Events::Listen("ChronoDeactivated", [this](void* data) {
            (void)data;
            EnableWalls();
        });

        LOG_DEBUG("Misc_WallToggle listening to Chrono events");
    }

    void Update(double deltaTime) override { (void)deltaTime; }
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_WallToggle"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void DisableWalls() { ToggleWalls(false); }
    void EnableWalls() { ToggleWalls(true); }

    void ToggleWalls(bool enable) {
        bool anyValid = false;
        for (const auto& wallRef : wallObjects) {
            if (!wallRef.IsValid()) {
                continue;
            }
            anyValid = true;
            RB_SetIsTrigger(!enable, wallRef.GetEntity());
        }
        if (!anyValid) {
            LOG_WARNING("Misc_WallToggle: wallObjects list is empty/invalid");
        }
    }

    std::vector<GameObjectRef> wallObjects; // preferred list
};
