#pragma once
#include "EngineAPI.hpp"
#include "Misc_PlayerRespawn.hpp"

/*
* Misc_PlayerRespawnTest
* Press T to freeze then teleport.
*/

class Misc_PlayerRespawnTest : public IScript {
public:
    Misc_PlayerRespawnTest() = default;
    ~Misc_PlayerRespawnTest() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(respawnEntityRef);
    }

    void Start() override {}

    void Update(double deltaTime) override {
        if (Input::WasKeyPressed('T')) {
            LOG_DEBUG("Misc_PlayerRespawnTest: T pressed");
            TriggerRespawn();
        }
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Misc_PlayerRespawnTest";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    GameObjectRef respawnEntityRef;
    bool warnedMissingRef = false;
    bool warnedMissingComponent = false;

    void TriggerRespawn() {
        if (!respawnEntityRef.IsValid()) {
            if (!warnedMissingRef) {
                LOG_WARNING("Misc_PlayerRespawnTest: missing respawn entity reference");
                warnedMissingRef = true;
            }
            return;
        }

        auto* respawn = GameObject(respawnEntityRef).GetComponent<Misc_PlayerRespawn>();
        if (!respawn) {
            if (!warnedMissingComponent) {
                LOG_WARNING("Misc_PlayerRespawnTest: entity has no Misc_PlayerRespawn");
                warnedMissingComponent = true;
            }
            return;
        }

        LOG_DEBUG("Misc_PlayerRespawnTest: calling RespawnNow");
        respawn->RespawnNow();
    }
};
