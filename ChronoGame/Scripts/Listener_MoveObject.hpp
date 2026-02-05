#pragma once
#include "Listener_.hpp"
#include "EngineAPI.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

// Slap this script onto the game object you want to move after something

class Listener_MoveObject : public Listener_ {
public:
    Listener_MoveObject() {
        SCRIPT_FIELD(startingPos, Vec3);
        SCRIPT_FIELD(targetPos, Vec3);
        SCRIPT_FIELD(tweenDuration, Float);
    }

    ~Listener_MoveObject() override = default;

    // === Custom Methods ===
    void MoveObject()
    {
        if (isMoving)
            return;

        isMoving = true;
        destinationReached = false;

        Tweener::StartVec3(
            [this](const Vec3& pos) {
                SetPosition(GetTransformRef(GetEntity()), pos);
            },
            startingPos,
            targetPos,
            tweenDuration,
            Tweener::Type::CUBIC_EASE_BOTH,
            GetEntity()
        );
    }

    void MoveObjectReversed()
    {
        if (isMoving)
            return;

        isMoving = true;
        destinationReached = false;

        Tweener::StartVec3(
            [this](const Vec3& pos) {
                SetPosition(GetTransformRef(GetEntity()), pos);
            },
            targetPos,
            startingPos,
            tweenDuration,
            Tweener::Type::CUBIC_EASE_BOTH,
            GetEntity()
        );
    }

    void Solve() override {
        MoveObject();
    };
    void Unsolve() override {
        MoveObjectReversed();
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
        // Called every frame while the script is enabled
        if (!destinationReached)
        {
            Vec3 currentPos = GetPosition(GetTransformRef(GetEntity()));
            float dist = (targetPos - currentPos).Length();
            if (dist < 1.0f)
            {
                destinationReached = true;
                isMoving = false;
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
        return "Listener_MoveObject";
    }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Vec3 startingPos;
    Vec3 targetPos;
    bool isMoving = false;
    bool destinationReached = false;
    float tweenDuration = 1.5f;
};