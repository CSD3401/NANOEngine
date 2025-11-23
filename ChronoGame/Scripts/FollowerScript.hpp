#pragma once
#include "EngineAPI.hpp"

/**
 * Example: Follower Script
 * This entity will follow another entity's Transform component.
 *
 * Usage:
 * 1. Add this script to an entity (e.g., "Enemy")
 * 2. In the inspector, drag the "Player" entity onto the "targetTransform" field
 * 3. The enemy will now follow the player!
 */
class FollowerScript : public IScript {
public:
    FollowerScript() {
// Register component reference - this will show up in the inspector
        SCRIPT_COMPONENT_REF(targetTransform, TransformRef);

        // Register other fields
        SCRIPT_FIELD(followSpeed, Float);
        SCRIPT_FIELD(stopDistance, Float);
    }

    void Initialize(Entity entity) override {
        // Nothing to do here
    }

    void Update(double deltaTime) override {
        // Check if we have a valid target
        if (!targetTransform.IsValid()) {
            return; // No target assigned
        }

        // Get target position using SDK method
        Vec3 targetPos = GetPosition(targetTransform);
        Vec3 myPos = GetPosition();

        // Calculate direction to target
        Vec3 direction = {
            targetPos.x - myPos.x,
            targetPos.y - myPos.y,
            targetPos.z - myPos.z
        };

        // Calculate distance
        float distance = std::sqrt(
            direction.x * direction.x +
            direction.y * direction.y +
            direction.z * direction.z
        );

        // Only move if we're far enough away
        if (distance > stopDistance) {
            // Normalize direction
            direction.x /= distance;
            direction.y /= distance;
            direction.z /= distance;

            // Move towards target
            float moveAmount = followSpeed * static_cast<float>(deltaTime);
            Translate(
                direction.x * moveAmount,
                direction.y * moveAmount,
                direction.z * moveAmount
            );

        }
    }

 const char* GetTypeName() const override {
        return "FollowerScript";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // Component reference - will hold reference to target's Transform
    TransformRef targetTransform;

    // Regular fields
    float followSpeed = 5.0f;
    float stopDistance = 2.0f;
};
