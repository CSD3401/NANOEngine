#pragma once
#include "EngineAPI.hpp"

/**
 * GameObject Reference Test Script
 *
 * Demonstrates script-to-script communication using GameObjectRef and GameObject.
 *
 * Features:
 * - GameObjectRef field with drag-drop support in editor
 * - Finding entities by name using GameObject::Find()
 * - Accessing other scripts' member functions and variables via GetComponent<T>()
 *
 * Usage:
 * 1. Add this script to an entity (e.g., "Player")
 * 2. Add another script (e.g., HealthScript) to a target entity (e.g., "Enemy")
 * 3. Drag the target entity onto the "target" field in the inspector
 * 4. Press Play to see script-to-script communication in action
 */
class GameObjectTest : public IScript {
public:
    GameObjectTest() = default;

    void Initialize(Entity entity) override {
        // Register GameObjectRef field - drag entity from hierarchy in editor!
        SCRIPT_GAMEOBJECT_REF(target);

        // Register other fields
        SCRIPT_FIELD(detectionRange, Float);
        SCRIPT_FIELD(moveSpeed, Float);
        SCRIPT_FIELD(debugEnabled, Bool);
    }

    void Start() override {
        LOG_DEBUG("GameObjectTest started on entity " << GetEntity());
    }

    void Update(double deltaTime) override {
        // If no target assigned, try to find one by name
        if (!target.IsValid()) {
            static bool searchedOnce = false;
            if (!searchedOnce) {
                // Example: Find an entity named "Enemy" in the scene
                GameObjectRef foundTarget;
                foundTarget.SetEntity(NE::Scripting::GameObject::Find("Enemy").GetEntityId());
                if (foundTarget.IsValid()) {
                    target = foundTarget;
                    LOG_DEBUG("Auto-found Enemy entity!");
                }
                searchedOnce = true;
            }
            return;
        }

        // Get positions using IScript methods
        Vec3 myPos = GetPosition();
        Vec3 targetPos = GetPosition(target.GetEntity());

        // Calculate distance to target
        Vec3 diff = {
            targetPos.x - myPos.x,
            targetPos.y - myPos.y,
            targetPos.z - myPos.z
        };
        float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // Move towards target if in detection range
        if (distance < detectionRange && distance > 1.0f) {
            Vec3 direction = {
                diff.x / distance,
                diff.y / distance,
                diff.z / distance
            };

            float moveAmount = moveSpeed * static_cast<float>(deltaTime);
            // Use IScript method for movement
            Translate(
                direction.x * moveAmount,
                direction.y * moveAmount,
                direction.z * moveAmount
            );
        }
    }

    void OnCollisionEnter(Entity other) override {
        LOG_DEBUG("Collided with entity: " << other);

        // Example: Set target dynamically on collision
        if (!target.IsValid()) {
            target.SetEntity(other);
            LOG_DEBUG("Target set to collision entity: " << other);
        }

        // Example: Access script on the entity we collided with
        // GameObject otherGO(other);
        // auto otherScript = otherGO.GetComponent<SomeScript>();
        // if (otherScript) {
        //     otherScript->DoSomething();
        // }
    }

    void OnTriggerEnter(Entity other) override {
        LOG_DEBUG("Trigger enter with entity: " << other);
    }

    void OnTriggerExit(Entity other) override {
        LOG_DEBUG("Trigger exit with entity: " << other);
    }

    // IScript Interface (required overrides)
    void OnCollisionExit(Entity other) override {}

    const char* GetTypeName() const override {
        return "GameObjectTest";
    }

    //=====================================================================
    // PUBLIC API (for other scripts to call)
    //=====================================================================

    /**
     * Example of a public function that other scripts can call.
     * This demonstrates how scripts can access each other's member functions.
     */
    void TakeDamage(int amount) {
        LOG_DEBUG("GameObjectTest took damage: " << amount);
        // Could reduce health, play sound, etc.
    }

    int GetDistanceToTarget() const {
        if (!target.IsValid()) {
            return -1;
        }
        Vec3 myPos = GetPosition();
        Vec3 targetPos = GetPosition(target.GetEntity());
        Vec3 diff = { targetPos.x - myPos.x, targetPos.y - myPos.y, targetPos.z - myPos.z };
        return static_cast<int>(std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z));
    }

private:
    //=====================================================================
    // MEMBER VARIABLES (accessible by other scripts via GetComponent)
    //=====================================================================

    // GameObjectRef - drag entity from hierarchy in editor!
    // This allows other scripts to be referenced and accessed
    GameObjectRef target;

    // Editable fields (settable in editor)
    float detectionRange = 10.0f;
    float moveSpeed = 3.0f;
    bool debugEnabled = true;

    // Private variables
    int health = 100;
};
