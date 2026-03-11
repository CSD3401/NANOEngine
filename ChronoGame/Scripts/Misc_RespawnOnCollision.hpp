#pragma once
#include "EngineAPI.hpp"
#include "Misc_PlayerRespawn.hpp"

/*
* Misc_RespawnOnCollision
* Triggers player respawn when the player collides or triggers the this entity.
* Collision is ignored if the entity is inactive.
*/

class Misc_RespawnOnCollision : public IScript {
public:
    Misc_RespawnOnCollision() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_GAMEOBJECT_REF(respawnRef);
    }

    ~Misc_RespawnOnCollision() override = default;

    // === Lifecycle Methods ===
    void Awake() override {
        ValidateReferences();
    }

    void Initialize(Entity entity) override {
    }
    void Start() override {
        ValidateReferences();
    }
    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override { ValidateReferences(); }

    const char* GetTypeName() const override {
        return "Misc_RespawnOnCollision";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {
        HandlePlayerEnter(other);
    }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override {
        HandlePlayerEnter(other);
    }
    void OnTriggerExit(Entity other) override {
        (void)other;
    }
    void OnTriggerStay(Entity other) override {
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

inline void Misc_RespawnOnCollision::ValidateReferences() {
    CacheRespawn();
    if (!playerRef.IsValid()) {
        LOG_WARNING("Misc_RespawnOnCollision: missing Player reference");
    }
    if (!respawnRef.IsValid()) {
        LOG_WARNING("Misc_RespawnOnCollision: missing Respawn reference");
    }
}

inline void Misc_RespawnOnCollision::CacheRespawn() {
    if (!respawnRef.IsValid()) {
        respawn = nullptr;
        return;
    }
    respawn = GameObject(respawnRef).GetComponent<Misc_PlayerRespawn>();
    if (!respawn) {
        LOG_WARNING("Misc_RespawnOnCollision: Respawn entity has no Misc_PlayerRespawn");
    }
}

inline void Misc_RespawnOnCollision::HandlePlayerEnter(Entity other) {
    if (!IsActiveInHierarchy()) {
        return;
    }
    if (!playerRef.IsValid() || !respawnRef.IsValid()) {
        LOG_WARNING("Misc_RespawnOnCollision: missing Player or Respawn reference");
        return;
    }
    if (other != playerRef.GetEntity()) {
        return;
    }
    if (!respawn) {
        CacheRespawn();
    }
    if (!respawn) {
        LOG_WARNING("Misc_RespawnOnCollision: respawn component not found");
        return;
    }
    respawn->RespawnNow();
}
