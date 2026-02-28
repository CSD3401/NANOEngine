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
        SCRIPT_GAMEOBJECT_REF(leftLaser1);
        SCRIPT_GAMEOBJECT_REF(leftLaser2);
        SCRIPT_GAMEOBJECT_REF(leftLaser3);
        SCRIPT_GAMEOBJECT_REF(leftLaser4);
        SCRIPT_GAMEOBJECT_REF(leftLaser5);
        SCRIPT_GAMEOBJECT_REF(leftLaser6);
        SCRIPT_GAMEOBJECT_REF(leftLaser7);
        SCRIPT_GAMEOBJECT_REF(leftLaser8);
        SCRIPT_GAMEOBJECT_REF(rightLaser);
        SCRIPT_GAMEOBJECT_REF(rightLaser1);
        SCRIPT_GAMEOBJECT_REF(rightLaser2);
        SCRIPT_GAMEOBJECT_REF(rightLaser3);
        SCRIPT_GAMEOBJECT_REF(rightLaser4);
        SCRIPT_GAMEOBJECT_REF(rightLaser5);
        SCRIPT_GAMEOBJECT_REF(rightLaser6);
        SCRIPT_GAMEOBJECT_REF(rightLaser7);
        SCRIPT_GAMEOBJECT_REF(rightLaser8);
        
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
        //SetActive(false, leftLaserEntity);
        //SetActive(false, rightLaserEntity); 

        Anim_Play(leftLaser1.GetEntity());
        Anim_Play(leftLaser2.GetEntity());
        Anim_Play(leftLaser3.GetEntity());
        Anim_Play(leftLaser4.GetEntity());
        Anim_Play(leftLaser5.GetEntity());
        Anim_Play(leftLaser6.GetEntity());
        Anim_Play(leftLaser7.GetEntity());
        Anim_Play(leftLaser8.GetEntity());

        Anim_Play(rightLaser1.GetEntity());
        Anim_Play(rightLaser2.GetEntity());
        Anim_Play(rightLaser3.GetEntity());
        Anim_Play(rightLaser4.GetEntity());
        Anim_Play(rightLaser5.GetEntity());
        Anim_Play(rightLaser6.GetEntity());
        Anim_Play(rightLaser7.GetEntity());
        Anim_Play(rightLaser8.GetEntity());

        RB_SetIsTrigger(true, leftLaserEntity);
        RB_SetIsTrigger(true, rightLaserEntity);
    }

    GameObjectRef leftLaser;
    GameObjectRef leftLaser1;
    GameObjectRef leftLaser2;
    GameObjectRef leftLaser3;
    GameObjectRef leftLaser4;
    GameObjectRef leftLaser5;
    GameObjectRef leftLaser6;
    GameObjectRef leftLaser7;
    GameObjectRef leftLaser8;

    GameObjectRef rightLaser;
    GameObjectRef rightLaser1;
    GameObjectRef rightLaser2;
    GameObjectRef rightLaser3;
    GameObjectRef rightLaser4;
    GameObjectRef rightLaser5;
    GameObjectRef rightLaser6;
    GameObjectRef rightLaser7;
    GameObjectRef rightLaser8;

    // Exposed fields
    std::string eventName = "PuzzleSolved1"; // PuzzleSolved1 - for puzzle wire
};