#pragma once
#include "EngineAPI.hpp"

/**
 * Example_EnumField - Demonstrates enum field usage in scripts
 *
 * This script shows how to expose enum fields to the editor using
 * either the SCRIPT_ENUM_FIELD macro or RegisterEnumField directly.
 * Enum fields appear as dropdowns in the Inspector panel.
 */

// Define your enums at file scope (outside the class)
enum class AIState {
    Idle = 0,
    Patrol,
    Chase,
    Attack,
    Flee
};

enum class MovementMode {
    Walk = 0,
    Run,
    Crouch,
    Swim
};

class Example_EnumField : public IScript {
public:
    Example_EnumField() = default;
    ~Example_EnumField() override = default;

    void Initialize(Entity entity) override {
        // Method 1: Using the SCRIPT_ENUM_FIELD macro (recommended)
        // The macro automatically uses the field name and creates a dropdown
        SCRIPT_ENUM_FIELD(aiState, "Idle", "Patrol", "Chase", "Attack", "Flee");
        SCRIPT_ENUM_FIELD(movementMode, "Walk", "Run", "Crouch", "Swim");

        // Method 2: Using RegisterEnumField directly (more verbose but equivalent)
        // RegisterEnumField("aiState", &aiState, {"Idle", "Patrol", "Chase", "Attack", "Flee"});

        // Vector of enums - useful for state queues, behavior sequences, etc.
        SCRIPT_ENUM_VECTOR_FIELD(stateQueue, "Idle", "Patrol", "Chase", "Attack", "Flee");

        // You can also expose regular fields alongside enums
        SCRIPT_FIELD(detectionRange, Float);
        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(debugMode, Bool);
    }

    void Start() override {
        LOG_INFO("Example_EnumField started with AI state: {} and movement: {}",
                 static_cast<int>(aiState), static_cast<int>(movementMode));
    }

    void Update(double deltaTime) override {
        // Example: Different behavior based on enum state
        switch (aiState) {
            case AIState::Idle:
                // Do idle behavior
                break;
            case AIState::Patrol:
                // Do patrol behavior with current movement mode
                HandlePatrol(deltaTime);
                break;
            case AIState::Chase:
                // Chase target
                break;
            case AIState::Attack:
                // Attack target
                break;
            case AIState::Flee:
                // Run away
                break;
        }
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
        if (debugMode) {
            LOG_INFO("AI State changed to: {}", static_cast<int>(aiState));
            LOG_INFO("Movement Mode changed to: {}", static_cast<int>(movementMode));
        }
    }

    const char* GetTypeName() const override {
        return "Example_EnumField";
    }

    // Required collision callbacks (pure virtual in IScript)
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void HandlePatrol(double deltaTime) {
        float speed = moveSpeed;

        // Adjust speed based on movement mode
        switch (movementMode) {
            case MovementMode::Walk:
                speed *= 1.0f;
                break;
            case MovementMode::Run:
                speed *= 2.0f;
                break;
            case MovementMode::Crouch:
                speed *= 0.5f;
                break;
            case MovementMode::Swim:
                speed *= 0.75f;
                break;
        }

        // Use speed for movement...
        (void)speed;
        (void)deltaTime;
    }

    // Enum fields - these will appear as dropdowns in the Inspector
    AIState aiState = AIState::Idle;
    MovementMode movementMode = MovementMode::Walk;

    // Vector of enums - each element has its own dropdown
    std::vector<AIState> stateQueue;

    // Regular fields for comparison
    float detectionRange = 10.0f;
    float moveSpeed = 5.0f;
    bool debugMode = false;
};
