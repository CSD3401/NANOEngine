#pragma once
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include <Math/Vec3.hpp>

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
        SCRIPT_COMPONENT_REF(targetTransform, Transform);
      
        // Register other fields
        SCRIPT_FIELD(followSpeed, Float);
        SCRIPT_FIELD(stopDistance, Float);
    }

    void Initialize(NE::ECS::Entity entity) override {
        // Nothing to do here
    }

    void Update(double deltaTime) override {
        // Check if we have a valid target
        if (!targetTransform) {
         return; // No target assigned
        }

      // Get target position
        NE::Math::Vec3 targetPos = targetTransform->position;
        NE::Math::Vec3 myPos = GetPosition();
        
        // Calculate direction to target
        NE::Math::Vec3 direction = {
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
    void OnCollisionEnter(NE::ECS::Entity other) override {}
    void OnCollisionExit(NE::ECS::Entity other) override {}
    void OnTriggerEnter(NE::ECS::Entity other) override {}
    void OnTriggerExit(NE::ECS::Entity other) override {}

private:
    // ? Component reference - will hold pointer to target's Transform
    ComponentRef<NE::ECS::Component::Transform> targetTransform;
    
    // Regular fields
    float followSpeed = 5.0f;
    float stopDistance = 2.0f;
};
