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
class Interactable_Gate : public IScript {
public:
    Interactable_Gate() {
        SCRIPT_GAMEOBJECT_REF(playerRef);
        SCRIPT_FIELD(interactionDistance, Float);
        SCRIPT_FIELD(moveDistance, Float);
        SCRIPT_FIELD(tweenDuration, Float);
        SCRIPT_FIELD(logInteractions, Bool);
    }

    ~Interactable_Gate() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        gateEntity = GetEntity();
		startingPos = TF_GetLocalPosition(gateEntity);

        if (!playerRef.IsValid()) {
            LOG_ERROR("Interactable_Gate: playerRef not assigned!");
        }

        if (interactionDistance <= 0.0f) {
            LOG_WARNING("Interactable_Gate: interactionDistance must be > 0, setting to 5.0");
            interactionDistance = 5.0f;
        }

        if (tweenDuration <= 0.0f) {
            LOG_WARNING("Interactable_Gate: tweenDuration must be > 0, setting to 1.5");
            tweenDuration = 1.5f;
        }

        LOG_DEBUG("Interactable_Gate initialized - Distance: {}, Duration: {}, MoveDistance: {}",
            interactionDistance, tweenDuration, moveDistance);
    }

    void Update(double deltaTime) override {
        if (!playerRef.IsValid() || isMoving) return;

        // Get player entity and positions
        Entity player = playerRef.GetEntity();
        Vec3 playerPos = GetPosition(GetTransformRef(player));
        Vec3 gatePos = TF_GetPosition(gateEntity);

        // Calculate distance
        Vec3 delta = playerPos - gatePos;
        float distance = delta.Length();

        // Check if player is in range and presses E
        if (distance <= interactionDistance) {
            if (Input::WasKeyPressed('E')) {
                OpenGate();
            }
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Interactable_Gate"; }

    // === Collision Callbacks (required by IScript) ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void OpenGate() {
        PlayAudio("event:/DOOR_SLIDE");

        isMoving = true;

        // Calculate target position: move along Z axis by moveDistance
        Vec3 targetPos = startingPos;
        targetPos.x -= moveDistance;

        // Start tween from current position to target position
        Tweener::StartVec3(
            [this](const Vec3& pos) {
                //SetPosition(GetTransformRef(gateEntity), pos);
                TF_SetPosition(pos, gateEntity);
            },
            startingPos,
            targetPos,
            tweenDuration,
            Tweener::Type::CUBIC_EASE_BOTH,
            gateEntity
        );

        RB_SetIsTrigger(true, gateEntity);

        if (logInteractions) {
            LOG_DEBUG("Gate opening! Moving from ({}, {}, {}) to ({}, {}, {})",
                startingPos.x, startingPos.y, startingPos.z,
                targetPos.x, targetPos.y, targetPos.z);
        }
    }

    // Exposed fields
    GameObjectRef playerRef;
    float interactionDistance = 5.0f;
    float moveDistance = 1.265f;  // Default: moves 1.265 units on Z axis
    float tweenDuration = 1.5f;
    bool logInteractions = true;

    // Runtime
    Entity gateEntity = 0;
    Vec3 startingPos;
    bool isMoving = false;
};