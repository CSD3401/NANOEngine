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
class LaserListener : public IScript {
public:
    LaserListener() {
        SCRIPT_GAMEOBJECT_REF(leftLaser);
        SCRIPT_GAMEOBJECT_REF(rightLaser);
    }

    ~LaserListener() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        if (!eventName.empty()) {
            Events::Listen(eventName.c_str(), [this](void* data) {
                (void)data;
                DisableLaser();
                });
            LOG_DEBUG("Laser Listener'{}'", eventName);
        }
    }

    void Update(double deltaTime) override {



    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "LaserListener"; }

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
        Entity leftLaserEntity = leftLaser.GetEntity();
        Entity rightLaserEntity = rightLaser.GetEntity();
        SetActive(false, leftLaserEntity);
        SetActive(false, rightLaserEntity); 
        RB_SetIsTrigger(true, leftLaserEntity);
        RB_SetIsTrigger(true, rightLaserEntity);
    }

    GameObjectRef leftLaser;
    GameObjectRef rightLaser;

    // Exposed fields
    std::string eventName = "PuzzleSolved1"; // PuzzleSolved1 - for puzzle wire
};