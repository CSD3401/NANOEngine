#pragma once
#include "EngineAPI.hpp"

/**
 * Interactable_Gate
 *
 * Gate that smoothly moves along Z axis when player presses E nearby.
 *
 * Setup:
 * 1. Add this script to your gate entity
 * 2. Assign playerRef (drag Player entity)
 * 3. Set moveDistance (how far gate moves on Z axis)
 * 4. Set interactionDistance and tweenDuration
 * 5. Press E when near gate to open it
 */
class IntersectionListener : public IScript {
public:
    IntersectionListener() {
        SCRIPT_GAMEOBJECT_REF(laser);
        SCRIPT_GAMEOBJECT_REF(animTarget1);
        SCRIPT_GAMEOBJECT_REF(animTarget2);
        SCRIPT_GAMEOBJECT_REF(animTarget3);
        SCRIPT_GAMEOBJECT_REF(animTarget4);
        SCRIPT_GAMEOBJECT_REF(animTarget5);
        SCRIPT_GAMEOBJECT_REF(animTarget6);
        SCRIPT_GAMEOBJECT_REF(animTarget7);
        SCRIPT_GAMEOBJECT_REF(animTarget8);
        SCRIPT_FIELD(disableAfterSolve, Bool);
    }

    ~IntersectionListener() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        if (!eventName1.empty()) {
            Events::Listen(eventName1.c_str(), [this](void* data) {
                (void)data;
                receivedPuzzleSolved2 = true;
                LOG_DEBUG("Listened to PuzzleSolved2");

                });
        }

        if (!eventName2.empty()) {
            Events::Listen(eventName2.c_str(), [this](void* data) {
                (void)data;
                receivedRaziPuzzle = true;
                LOG_DEBUG("Listened to RaziPuzzle");

                });
        }
    }

    void Update(double deltaTime) override {

        if (doOnce)
			return;

        if (receivedPuzzleSolved2 && receivedRaziPuzzle)
        {
            DisableLaser();
            doOnce = true;
        }

    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "IntersectionListener"; }

    // === Collision Callbacks (required by IScript) ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:

    void DisableLaser()
    {
        LOG_DEBUG("IntersectionListener - DisableLaser");
        // Play assigned laser open animations first.
        AnimPlayIfValid(animTarget1);
        AnimPlayIfValid(animTarget2);
        AnimPlayIfValid(animTarget3);
        AnimPlayIfValid(animTarget4);
        AnimPlayIfValid(animTarget5);
        AnimPlayIfValid(animTarget6);
        AnimPlayIfValid(animTarget7);
        AnimPlayIfValid(animTarget8);

        if (!laser.IsValid()) {
            LOG_WARNING("IntersectionListener: `laser` is not assigned.");
            return;
        }

        Entity laserEntity = laser.GetEntity();
        RB_SetIsTrigger(true, laserEntity);

        if (disableAfterSolve) {
            SetActive(false, laserEntity);
        }
    }

    void AnimPlayIfValid(const GameObjectRef& ref)
    {
        if (ref.IsValid()) {
            Anim_Play(ref.GetEntity());
        }
    }

    bool receivedPuzzleSolved2 = false;
	bool receivedRaziPuzzle = false;
    bool doOnce = false;

    GameObjectRef laser;
    GameObjectRef animTarget1;
    GameObjectRef animTarget2;
    GameObjectRef animTarget3;
    GameObjectRef animTarget4;
    GameObjectRef animTarget5;
    GameObjectRef animTarget6;
    GameObjectRef animTarget7;
    GameObjectRef animTarget8;
    bool disableAfterSolve = false;

    // Exposed fields

    // listening to 2 events
    std::string eventName1 = "PuzzleSolved2"; 
    std::string eventName2 = "RaziPuzzle"; 
};