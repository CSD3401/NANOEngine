#pragma once
#include "EngineAPI.hpp"
#include "Misc_PlayerRespawn.hpp"

/*
* Puzzle_Sinkhole
* Toggles present/past parent objects when the stopwatch is activated/deactivated.
* Present objects are hidden when ChronoActivated (past), and shown when ChronoDeactivated (present).
*/

class Misc_Sinkhole : public IScript {
public:
    Misc_Sinkhole() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(respawnRef);
    }

    ~Misc_Sinkhole() override = default;

    // === Lifecycle Methods ===
    void Awake() override {
        ValidateReferences();
    }

    void Initialize(Entity entity) override {
    }
    void Start() override {
        ValidateReferences();
        LOG_DEBUG("Misc_Sinkhole: has collider=" << Query::HasCollider(GetEntity())
            << " rigidbody=" << Query::HasRigidbody(GetEntity()));
        if (playerRef.IsValid()) {
            const Entity playerEntity = playerRef.GetEntity();
            LOG_DEBUG("Misc_Sinkhole: player has collider=" << Query::HasCollider(playerEntity)
                << " rigidbody=" << Query::HasRigidbody(playerEntity));
        }
    }
    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override { LOG_DEBUG("Misc_Sinkhole: OnEnable"); }
    void OnDisable() override {}
    void OnValidate() override { ValidateReferences(); }

    const char* GetTypeName() const override {
        return "Misc_Sinkhole";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {
        LOG_DEBUG("Misc_Sinkhole: OnCollisionEnter");
        HandlePlayerEnter(other);
    }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override {
        LOG_DEBUG("Misc_Sinkhole: OnTriggerEnter");
        HandlePlayerEnter(other);
    }
    void OnTriggerExit(Entity other) override {
        LOG_DEBUG("Misc_Sinkhole: OnTriggerExit");
        (void)other;
    }
    void OnTriggerStay(Entity other) override {
        LOG_DEBUG("Misc_Sinkhole: OnTriggerStay");
        (void)other;
    }

private:
    GameObjectRef playerRef;
    GameObjectRef respawnRef;
    Misc_PlayerRespawn* respawn = nullptr;

    void ValidateReferences();
    void CacheRespawn();
    void HandlePlayerEnter(Entity other);
};

inline void Misc_Sinkhole::ValidateReferences() {
    CacheRespawn();
    if (!playerRef.IsValid()) {
        LOG_WARNING("Misc_Sinkhole: missing Player reference");
    }
    if (!respawnRef.IsValid()) {
        LOG_WARNING("Misc_Sinkhole: missing Respawn reference");
    }
}

inline void Misc_Sinkhole::CacheRespawn() {
    if (!respawnRef.IsValid()) {
        respawn = nullptr;
        return;
    }
    respawn = GameObject(respawnRef).GetComponent<Misc_PlayerRespawn>();
    if (!respawn) {
        LOG_WARNING("Misc_Sinkhole: Respawn entity has no Misc_PlayerRespawn");
    }
}

inline void Misc_Sinkhole::HandlePlayerEnter(Entity other) {
    LOG_DEBUG("Misc_Sinkhole: collision/trigger enter");
    if (!playerRef.IsValid() || !respawnRef.IsValid()) {
        LOG_WARNING("Misc_Sinkhole: missing Player or Respawn reference");
        return;
    }
    LOG_DEBUG("Misc_Sinkhole: other has collider=" << Query::HasCollider(other)
        << " rigidbody=" << Query::HasRigidbody(other));
    LOG_DEBUG("Misc_Sinkhole: other=" << other << " player=" << playerRef.GetEntity());
    if (other != playerRef.GetEntity()) {
        LOG_DEBUG("Misc_Sinkhole: hit non-player entity");
        return;
    }
    LOG_DEBUG("Misc_Sinkhole: player detected");
    if (!respawn) {
        CacheRespawn();
    }
    if (!respawn) {
        LOG_WARNING("Misc_Sinkhole: respawn component not found");
        return;
    }
    LOG_DEBUG("Misc_Sinkhole: respawn now");
    respawn->RespawnNow();
}
