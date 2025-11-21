#pragma once
#include "EngineAPI.hpp"

/**
 * Example: Parent Controller Script
 * Demonstrates how to access and manipulate child entities from a parent entity.
 *
 * Usage:
 * 1. Add this script to a parent entity that has one or more child entities
 * 2. The script will log information about all children
 * 3. You can enable/disable rotation to spin all children around the parent
 *
 * Use Cases:
 * - Control multiple child objects from a single parent script
 * - Apply transformations to all children at once
 * - Enable/disable child entities programmatically
 * - Access child components for gameplay logic
 */
class ParentControllerScript : public IScript {
public:
    ParentControllerScript() {
        // Register fields that will appear in the inspector
        SCRIPT_FIELD(rotateChildren, Bool);
        SCRIPT_FIELD(rotationSpeed, Float);
        SCRIPT_FIELD(scaleChildren, Bool);
        SCRIPT_FIELD(scaleAmount, Float);
    }

    void Initialize(Entity entity) override {
        
    }

    void Start() override {

        // Log how many children this entity has
        size_t childCount = GetChildCount();
        LOG_INFO("ParentController initialized with " << childCount << " children");

        // Log information about each child
        for (size_t i = 0; i < childCount; ++i) {
            Entity childEntity = GetChild(i);
            LOG_INFO("  Child " << i << " has entity ID: " << childEntity);
        }

        initialRotation = GetRotation();
    }

    void Update(double deltaTime) override {
        // Example 1: Rotate all children around the parent
        if (rotateChildren) {
            RotateAllChildren(deltaTime);
        }

        // Example 2: Scale all children together
        if (scaleChildren) {
            ScaleAllChildren();
        }

        // Example 3: Toggle children on/off with a key press
        if (Input::WasKeyPressed('T')) {
            ToggleChildren();
        }

        // Example 4: Access specific child by index
        if (Input::WasKeyPressed('1')) {
            AccessFirstChild();
        }
    }

    const char* GetTypeName() const override {
        return "ParentControllerScript";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // Configuration fields
    bool rotateChildren = false;
    float rotationSpeed = 45.0f;  // degrees per second
    bool scaleChildren = false;
    float scaleAmount = 1.0f;

    // Internal state
    Vec3 initialRotation = Vec3::Zero();
    bool childrenEnabled = true;
    float accumulatedRotation = 0.0f;

    /**
     * Rotate all child entities around the Y axis
     */
    void RotateAllChildren(double deltaTime) {
        accumulatedRotation += rotationSpeed * static_cast<float>(deltaTime);

        // Get all children using GetChildren()
        std::vector<Entity> children = GetChildren();

        for (Entity childEntity : children) {
            // Get a transform reference for each child
            TransformRef childTransform = GetTransformRef(childEntity);

            if (childTransform.IsValid()) {
                // Rotate the child
                Vec3 currentRotation = GetRotation(childTransform);
                currentRotation.y = initialRotation.y + accumulatedRotation;
                SetRotation(childTransform, currentRotation);
            }
        }
    }

    /**
     * Scale all child entities uniformly
     */
    void ScaleAllChildren() {
        size_t childCount = GetChildCount();

        for (size_t i = 0; i < childCount; ++i) {
            Entity childEntity = GetChild(i);
            TransformRef childTransform = GetTransformRef(childEntity);

            if (childTransform.IsValid()) {
                Vec3 newScale = Vec3(scaleAmount, scaleAmount, scaleAmount);
                SetScale(childTransform, newScale);
            }
        }
    }

    /**
     * (example of accessing child entities)
     */
    void ToggleChildren() {


        std::vector<Entity> children = GetChildren();

        // This is a placeholder example.
        for (Entity childEntity : children) {
            LOG_INFO("  Child entity: " << childEntity);
        }
    }

    /**
     * Access the first child and move it independently
     */
    void AccessFirstChild() {
        if (GetChildCount() > 0) {
            Entity firstChild = GetChild(0);
            TransformRef childTransform = GetTransformRef(firstChild);

            if (childTransform.IsValid()) {
                Vec3 currentPos = GetPosition(childTransform);
                currentPos.y += 1.0f;  // Move up
                SetPosition(childTransform, currentPos);

                LOG_INFO("Moved first child to: " << currentPos.x << ", " << currentPos.y << ", " << currentPos.z);
            }
        }
    }
};
