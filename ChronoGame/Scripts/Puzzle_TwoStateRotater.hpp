#pragma once
#include "EngineAPI.hpp"

/*
* Puzzle_Two State Rotater
* Two-state rotation tween on a target entity, driven by public methods.
*/

class Puzzle_TwoStateRotater : public IScript {
public:
    Puzzle_TwoStateRotater() {
        SCRIPT_GAMEOBJECT_REF(target);
        SCRIPT_FIELD(rotationX1, Float);
        SCRIPT_FIELD(rotationX2, Float);
        SCRIPT_FIELD(duration, Float);
        SCRIPT_FIELD(startingState, Bool);
    }

    ~Puzzle_TwoStateRotater() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {}

    void Start() override {
        currentState = startingState;
        CacheTarget();
        if (!targetTransformRef.IsValid()) {
            LOG_WARNING("Puzzle_TwoStateRotater: target not set or invalid");
            return;
        }

        Vec3 initialRotation = GetRotation(targetTransformRef);
        firstState = Vec3(rotationX1, initialRotation.y, initialRotation.z);
        secondState = Vec3(rotationX2, initialRotation.y, initialRotation.z);
        ApplyStateImmediate();
    }

    void Update(double deltaTime) override {}

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Puzzle_Two State Rotater";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

    // === Public API (for other scripts or events) ===
    void SwitchState() {
        currentState = !currentState;
        LOG_INFO("Puzzle_TwoStateRotater: SwitchState -> " << (currentState ? "state1" : "state2"));
        ApplyStateTweened();
    }

    void SetState(bool state) {
        currentState = state;
        LOG_INFO("Puzzle_TwoStateRotater: SetState -> " << (currentState ? "state1" : "state2"));
        ApplyStateTweened();
    }

    bool GetStartingState() const {
        return startingState;
    }

private:
    GameObjectRef target;
    TransformRef targetTransformRef;
    float rotationX1 = 0.0f;
    float rotationX2 = 0.0f;
    float duration = 0.0f;
    bool startingState = false;

    Vec3 firstState = Vec3::Zero(); // Quaternion
    Vec3 secondState = Vec3::Zero(); // Quaternion
    bool currentState = false;

    void CacheTarget() {
        if (target.IsValid()) {
            targetTransformRef = GetTransformRef(target.GetEntity());
        }
        else {
            targetTransformRef = TransformRef();
        }
    }

    Vec3 GetTargetRotation() const {
        return currentState ? firstState : secondState;
    }

    void ApplyStateImmediate() {
        if (!targetTransformRef.IsValid()) {
            return;
        }
        SetRotation(targetTransformRef, GetTargetRotation());
    }

    void ApplyStateTweened() {
        if (!targetTransformRef.IsValid()) {
            LOG_WARNING("Puzzle_TwoStateRotater: target invalid while applying state");
            return;
        }

        Vec3 startRotation = GetRotation(targetTransformRef);
        Vec3 endRotation = GetTargetRotation();

        if (duration <= 0.0f) {
            SetRotation(targetTransformRef, endRotation);
            return;
        }

        Tweener::StartVec3(
            [this](const Vec3& rot) {
                SetRotation(targetTransformRef, rot);
            },
            startRotation,
            endRotation,
            duration,
            Tweener::Type::CUBIC_EASE_IN,
            GetEntity()
        );
    }
};
