#pragma once
#include "EngineAPI.hpp"

/*
* Misc_PlayerRespawn
* Handles player death + respawn with optional screen fade.
* Teleports the player instantly to the checkpoint position.
*/

class Misc_PlayerRespawn : public IScript {
public:
    Misc_PlayerRespawn() = default;
    ~Misc_PlayerRespawn() override = default;

    // === Lifecycle Methods ===
    void Awake() override { ValidateReferences(); }

    void Initialize(Entity entity) override {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(checkpointRef);
    }

    void Start() override { ValidateReferences(); }

    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override { ValidateReferences(); }

    const char* GetTypeName() const override {
        return "Misc_PlayerRespawn";
    }

    void RespawnNow() {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Player or Checkpoint reference");
            return;
        }
        TeleportToCheckpoint();
        PlayAudio("event:/ELECTRIC_SHOCK"); // REPLACE THIS - RF
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef playerRef;
    GameObjectRef checkpointRef;

    void ValidateReferences() {
        if (!playerRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Player reference");
        }
        if (!checkpointRef.IsValid()) {
            LOG_WARNING("Misc_PlayerRespawn: missing Checkpoint reference");
        }
    }

    void HandlePlayerEnter(Entity other) {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            return;
        }
        if (other != playerRef.GetEntity()) {
            return;
        }
        TeleportToCheckpoint();
    }

    void TeleportToCheckpoint() {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            return;
        }
        const Entity playerEntity = playerRef.GetEntity();
        const Entity checkpointEntity = checkpointRef.GetEntity();
        Vec3 checkpointPos = TF_GetPosition(checkpointEntity);
        CC_SetPosition(checkpointPos, playerEntity);
        LOG_DEBUG(("Misc_PlayerRespawn: teleported to checkpoint (" +
            std::to_string(checkpointPos.x) + ", " +
            std::to_string(checkpointPos.y) + ", " +
            std::to_string(checkpointPos.z) + ")").c_str());
    }
};
