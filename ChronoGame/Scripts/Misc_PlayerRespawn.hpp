#pragma once
#include "EngineAPI.hpp"
#include <cmath>

/*
* Misc_PlayerRespawn
* Handles player death + respawn with optional screen fade.
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
        SCRIPT_FIELD(returnSpeed, Float);
        SCRIPT_FIELD(returnOnlyXZ, Bool);
        SCRIPT_FIELD(returnStopThreshold, Float);
    }

    void Start() override { ValidateReferences(); }

    void Update(double deltaTime) override {
        if (!isReturning) {
            return;
        }
        if (pendingTeleportEntity == NE::Scripting::INVALID_ENTITY) {
            isReturning = false;
            return;
        }

        Vec3 currentPos = TF_GetPosition(pendingTeleportEntity);
        Vec3 toTarget = Vec3(
            pendingTeleportPos.x - currentPos.x,
            pendingTeleportPos.y - currentPos.y,
            pendingTeleportPos.z - currentPos.z
        );
        if (returnOnlyXZ) {
            toTarget.y = 0.0f;
        }
        float distSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
        float thresholdSq = returnStopThreshold * returnStopThreshold;
        if (distSq <= thresholdSq) {
            isReturning = false;
            return;
        }
        float dist = std::sqrt(distSq);
        float step = returnSpeed * static_cast<float>(deltaTime);
        if (step >= dist) {
            CC_Move(toTarget, pendingTeleportEntity);
            isReturning = false;
            return;
        }
        float scale = step / dist;
        Vec3 delta = Vec3(toTarget.x * scale, toTarget.y * scale, toTarget.z * scale);
        CC_Move(delta, pendingTeleportEntity);
    }

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
        const Entity playerEntity = playerRef.GetEntity();
        Vec3 beforePos = TF_GetPosition(playerEntity);
        // LOG_DEBUG(("Misc_PlayerRespawn: player pos before (" +
        //     std::to_string(beforePos.x) + ", " +
        //     std::to_string(beforePos.y) + ", " +
        //     std::to_string(beforePos.z) + ")").c_str());
        BeginReturn();
        Vec3 afterPos = TF_GetPosition(playerEntity);
        // LOG_DEBUG(("Misc_PlayerRespawn: player pos after (" +
        //     std::to_string(afterPos.x) + ", " +
        //     std::to_string(afterPos.y) + ", " +
        //     std::to_string(afterPos.z) + ")").c_str());
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override { HandlePlayerEnter(other); }
    void OnTriggerExit(Entity other) override {}

private:
    GameObjectRef playerRef;
    GameObjectRef checkpointRef;
    float returnSpeed = 100.0f;
    bool returnOnlyXZ = true;
    float returnStopThreshold = 0.05f;
    bool isReturning = false;
    Entity pendingTeleportEntity = NE::Scripting::INVALID_ENTITY;
    Vec3 pendingTeleportPos = Vec3::Zero();

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

        BeginReturn();
    }

    void BeginReturn() {
        if (!playerRef.IsValid() || !checkpointRef.IsValid()) {
            return;
        }
        const Entity playerEntity = playerRef.GetEntity();
        const Entity checkpointEntity = checkpointRef.GetEntity();
        Vec3 checkpointPos = TF_GetPosition(checkpointEntity);
        pendingTeleportEntity = playerEntity;
        pendingTeleportPos = checkpointPos;
        isReturning = true;
        LOG_DEBUG(("Misc_PlayerRespawn: returning to checkpoint (" +
            std::to_string(checkpointPos.x) + ", " +
            std::to_string(checkpointPos.y) + ", " +
            std::to_string(checkpointPos.z) + ")").c_str());
    }
};
