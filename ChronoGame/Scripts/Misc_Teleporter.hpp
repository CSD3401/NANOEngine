#pragma once
#include "EngineAPI.hpp"
#include <map>
#include <vector>

class Misc_Teleporter : public IScript {
public:
    Misc_Teleporter() {
        SCRIPT_GAMEOBJECT_REF(player);
        SCRIPT_COMPONENT_REF(targetTransformRef, TransformRef);
        SCRIPT_FIELD(eventBased, Bool);
        SCRIPT_FIELD(eventName, String);
        SCRIPT_FIELD(impactAudioName, String);
    }

    ~Misc_Teleporter() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {}

    void Start() override {
        playerInZone = false;
        msgReceived = false;

        if (eventBased && !eventName.empty()) {
            Events::Listen(eventName.c_str(), [this](void*) {
                msgReceived = true;
                LOG_DEBUG("[Misc_Teleporter] Event received: " + eventName);
                TryTeleport();
                });
        }
    }

    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Misc_Teleporter";
    }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        if (!eventBased) {
            // Original behaviour - teleport immediately on trigger
            if (!impactAudioName.empty())
                PlayAudio("event:/" + impactAudioName);
            CC_SetPosition(GetPosition(targetTransformRef), player.GetEntity());
        }
        else {
            playerInZone = true;
            LOG_DEBUG("[Misc_Teleporter] Player entered zone, waiting for event: " + eventName);
            TryTeleport();
        }
    }

    void OnTriggerExit(Entity other) override {
        if (eventBased) {
            playerInZone = false;
            LOG_DEBUG("[Misc_Teleporter] Player left zone.");
        }
    }

    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef player;
    TransformRef targetTransformRef;
    bool eventBased = false;
    std::string eventName;

    bool playerInZone = false;
    bool msgReceived = false;
    std::string impactAudioName = "PLAYER_IMPACT";

    void TryTeleport() {
        if (playerInZone && msgReceived) {
            LOG_DEBUG("[Misc_Teleporter] Both conditions met - teleporting.");
            if (!impactAudioName.empty())
                PlayAudio("event:/" + impactAudioName);
            CC_SetPosition(GetPosition(targetTransformRef), player.GetEntity());
            // Reset so it can't fire again
            playerInZone = false;
            msgReceived = false;
        }
    }
};