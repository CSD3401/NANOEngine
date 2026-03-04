#pragma once
#include "EngineAPI.hpp"
#include "Player_Controller.hpp"

/*
* Misc_ExplosionCameraShakeOnCollision
* Attach this to the object that should trigger the explosion-style camera shake.
*
* When the player collides or triggers with this object, it sends an event that
* Camera_ExplosionShake listens for on the player camera.
*/
class Misc_ExplosionCameraShakeOnCollision : public IScript {
public:
    Misc_ExplosionCameraShakeOnCollision() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(shakeEventName, String);
        SCRIPT_FIELD(triggerOnce, Bool);
        SCRIPT_FIELD(playExplosionAudio, Bool);
        SCRIPT_FIELD(explosionAudioEvent, String);
    }

    ~Misc_ExplosionCameraShakeOnCollision() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { (void)entity; }

    void Start() override {
        if (!playerRef.IsValid()) {
            AutoFindPlayer();
        }
    }

    void Update(double deltaTime) override { (void)deltaTime; }
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_ExplosionCameraShakeOnCollision"; }

    void OnCollisionEnter(Entity other) override { HandlePlayerTouch(other); }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { HandlePlayerTouch(other); }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef playerRef;
    std::string shakeEventName = "ExplosionCameraShake";
    bool triggerOnce = true;
    bool playExplosionAudio = false;
    std::string explosionAudioEvent = "event:/ELECTRIC_SHOCK";
    bool hasTriggered = false;

    void AutoFindPlayer() {
        auto players = GameObject::FindObjectsOfType<Player_Controller>();
        if (players.empty()) {
            LOG_WARNING("Misc_ExplosionCameraShakeOnCollision: could not auto-find Player_Controller.");
            return;
        }

        if (players.size() > 1) {
            LOG_WARNING("Misc_ExplosionCameraShakeOnCollision: multiple Player_Controller found; using the first one.");
        }

        playerRef = GameObjectRef(players.begin()->GetEntityId());
    }

    void HandlePlayerTouch(Entity other) {
        if (triggerOnce && hasTriggered) {
            return;
        }

        if (!playerRef.IsValid()) {
            AutoFindPlayer();
            if (!playerRef.IsValid()) {
                return;
            }
        }

        if (other != playerRef.GetEntity()) {
            return;
        }

        if (!shakeEventName.empty()) {
            Events::Send(shakeEventName.c_str());
            LOG_DEBUG("Misc_ExplosionCameraShakeOnCollision: sent event '" + shakeEventName + "'.");
        }

        if (playExplosionAudio && !explosionAudioEvent.empty()) {
            PlayAudio(explosionAudioEvent);
        }

        hasTriggered = true;
    }
};
