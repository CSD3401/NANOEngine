#pragma once
#include "EngineAPI.hpp"

/**
 * Example script demonstrating the Tween API functionality
 * Shows how to use tweens with lambdas, Vec3, and floats
 */
class TweenExampleScript : public IScript {
public:
    TweenExampleScript() = default;

    void Initialize(Entity entity) override {
        // Register editable fields
        SCRIPT_FIELD(tweenDuration, Float);
        SCRIPT_FIELD(targetDistance, Float);

        // Store initial position
        initialPosition = GetPosition();
    }

    void Start() override {
        LOG_DEBUG("TweenExampleScript started on entity "<< GetEntity());
    }

    void Update(double deltaTime) override {
        // Example 1: Start a position tween with Vec3 when pressing '1'
        if (Input::WasKeyPressed('1')) {
            LOG_DEBUG("Starting Vec3 position tween");

            Vec3 startPos = GetPosition();
            Vec3 targetPos = Vec3(startPos.x + targetDistance, startPos.y, startPos.z);

            // Using Tweener::StartVec3 with lambda
            Tweener::StartVec3(
                [this](const Vec3& pos) {
                    SetPosition(pos);
                },
                startPos,
                targetPos,
                tweenDuration,
                Tweener::Type::CUBIC_EASE_IN,
                GetEntity()
            );
        }

        // Example 2: Start a lambda-based tween when pressing '2'
        if (Input::WasKeyPressed('2')) {
            LOG_DEBUG("Starting lambda tween");

            Vec3 startPos = GetPosition();
            Vec3 targetPos = Vec3(startPos.x, startPos.y + targetDistance, startPos.z);

            // Using Tweener::StartLambda - captures start and target, receives t from 0 to 1
            Tweener::StartLambda(
                [this, startPos, targetPos](float t) {
                    Vec3 currentPos = Vec3(
                        startPos.x + (targetPos.x - startPos.x) * t,
                        startPos.y + (targetPos.y - startPos.y) * t,
                        startPos.z + (targetPos.z - startPos.z) * t
                    );
                    SetPosition(currentPos);
                },
                tweenDuration,
                Tweener::Type::LINEAR,
                GetEntity()
            );
        }

        // Example 3: Start a rotation tween when pressing '3'
        if (Input::WasKeyPressed('3')) {
            LOG_DEBUG("Starting rotation tween");

            Vec3 startRot = GetRotation();
            Vec3 targetRot = Vec3(startRot.x, startRot.y + 360.0f, startRot.z);

            Tweener::StartVec3(
                [this](const Vec3& rot) {
                    SetRotation(rot);
                },
                startRot,
                targetRot,
                tweenDuration,
                Tweener::Type::CUBIC_EASE_BOTH,
                GetEntity()
            );
        }

        // Example 4: Start a scale tween when pressing '4'
        if (Input::WasKeyPressed('4')) {
            LOG_DEBUG("Starting scale tween");

            Vec3 startScale = GetScale();
            Vec3 targetScale = Vec3(startScale.x * 2.0f, startScale.y * 2.0f, startScale.z * 2.0f);

            Tweener::StartVec3(
                [this](const Vec3& scale) {
                    SetScale(scale);
                },
                startScale,
                targetScale,
                tweenDuration,
                Tweener::Type::EASE_OUT,
                GetEntity()
            );
        }

        // Example 5: Start a float tween (custom property) when pressing '5'
        if (Input::WasKeyPressed('5')) {
            LOG_DEBUG("Starting float tween");

            Tweener::StartFloat(
                [this](float value) {
                    customValue = value;
                    LOG_DEBUG("Custom value: "<< customValue);
                },
                0.0f,
                100.0f,
                tweenDuration,
                Tweener::Type::LINEAR,
                GetEntity()
            );
        }

        // Example 6: Complex tween with multiple steps
        if (Input::WasKeyPressed('6')) {
            LOG_DEBUG("Starting multi-step tween sequence");

            Vec3 currentPos = GetPosition();
            Vec3 upPos = Vec3(currentPos.x, currentPos.y + 5.0f, currentPos.z);
            Vec3 downPos = Vec3(currentPos.x, currentPos.y, currentPos.z);

            // First tween: move up
            Tweener::StartVec3(
                [this](const Vec3& pos) { SetPosition(pos); },
                currentPos,
                upPos,
                tweenDuration * 0.5f,
                Tweener::Type::EASE_OUT,
                GetEntity()
            );

            // After some delay, move back down (this is simplified - in real use you'd chain with callbacks)
        }

        // Check if entity has active tweens when pressing 'C'
        if (Input::WasKeyPressed('C')) {
            bool hasTweens = Tweener::CheckEntity(GetEntity());
            LOG_DEBUG("Entity " << GetEntity() << " has active tweens: "<< hasTweens ? "YES" : "NO");
        }

        // Stop all tweens on this entity when pressing 'X'
        if (Input::WasKeyPressed('X')) {
            LOG_DEBUG("Stopping all tweens on entity "<< GetEntity());
            Tweener::StopEntity(GetEntity());
        }

        // Clear all tweens in the system when pressing 'Z'
        if (Input::WasKeyPressed('Z')) {
            LOG_DEBUG("Clearing all tweens in the system");
            Tweener::Clear();
        }

        // Reset position when pressing 'R'
        if (Input::WasKeyPressed('R')) {
            LOG_DEBUG("Resetting position");
            SetPosition(initialPosition);
            SetRotation(Vec3(0, 0, 0));
            SetScale(Vec3(1, 1, 1));
        }
    }

    void OnDestroy() override {
        // Clean up any tweens associated with this entity
        Tweener::StopEntity(GetEntity());
    }

    // IScript Interface (required overrides)
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

    const char* GetTypeName() const override {
        return "TweenExampleScript";
    }

private:
    // Editable fields
    float tweenDuration = 2.0f;
    float targetDistance = 10.0f;

    // Internal state
    Vec3 initialPosition;
    float customValue = 0.0f;
};
