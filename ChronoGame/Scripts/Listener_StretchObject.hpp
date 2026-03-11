#pragma once
#include "pch.h"
#include "Listener_.hpp"
#include "EngineAPI.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

 // Slap this script onto the game object you want to move after something

class Listener_StretchObject : public Listener_ {
public:
    Listener_StretchObject() {
        SCRIPT_FIELD(startingPos, Vec3);
        SCRIPT_FIELD(targetPos, Vec3);
        SCRIPT_FIELD(tweenDuration, Float);
    }

    ~Listener_StretchObject() override = default;

    // === Custom Methods ===
    void Solve() override {
		StretchObject();
    };
    void Unsolve() override {
        // Do nothing
    };

    // === Lifecycle Methods ===
    void Awake() override {}

    void Initialize(Entity entity) override {
		Listener_::Initialize(entity);
    }

    void Start() override {
        Listener_::Start();
        startingPos = GetPosition(GetTransformRef(GetEntity()));
    }

    void Update(double deltaTime) override {
        if (!destinationReached)
        {
            Vec3 currentPos = GetPosition(GetTransformRef(GetEntity()));
            float dist = (targetPos - currentPos).Length();
            if (dist < 1.0f)
            {
                destinationReached = true;
                isMoving = false;

                Vec3 dummy = startingPos;
                startingPos = targetPos;
                targetPos = dummy;
            }
        }
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {
		Listener_::OnEnable();
    }

    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Listener_StretchObject";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void StretchObject()
    {
        isMoving = true;
        destinationReached = false;

        Vec3 A = startingPos;
        Vec3 B = targetPos;

        Vec3 dir = (B - A).Normalized();
        float fullLength = (B - A).Length();

        Tweener::StartVec3(
            [this, A, dir](const Vec3& v) {
                float currentLength = v.x;

                TransformRef t = GetTransformRef(GetEntity());

                // rotation
                float angle = atan2(dir.y, dir.x);
                SetRotation(t, angle);

                // position (center-pivot safe)
                SetPosition(t, A + dir * (currentLength * 0.5f));

                // scale
                Vec3 sc = GetScale(t);
                SetScale(t, Vec3(currentLength, sc.y, sc.z));
            },
            Vec3::Zero(),
            Vec3(fullLength, 0, 0),
            tweenDuration,
            Tweener::Type::CUBIC_EASE_IN,
            GetEntity()
        );
    }

private:
    Vec3 startingPos;
    Vec3 targetPos;
    bool isMoving = false;
    bool destinationReached = false;
    float tweenDuration = 1.5f;
};