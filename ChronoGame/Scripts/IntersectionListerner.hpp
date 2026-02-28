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
        Entity LaserEntity = laser.GetEntity();
        SetActive(false, LaserEntity);
        RB_SetIsTrigger(true, LaserEntity);
    }

    bool receivedPuzzleSolved2 = false;
	bool receivedRaziPuzzle = false;
    bool doOnce = false;

    GameObjectRef laser;

    // Exposed fields

    // listening to 2 events
    std::string eventName1 = "PuzzleSolved2"; 
    std::string eventName2 = "RaziPuzzle"; 
};