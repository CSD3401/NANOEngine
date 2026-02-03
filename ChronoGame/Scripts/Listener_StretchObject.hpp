#pragma once
#include "pch.h"
#include "EngineAPI.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

 // Slap this script onto the game object you want to move after something

class Listener_StretchObject : public IScript {
public:
    Listener_StretchObject() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
        SCRIPT_FIELD(startingPos, Vec3);
        SCRIPT_FIELD(targetPos, Vec3);
        SCRIPT_FIELD(tweenDuration, Float);
        SCRIPT_FIELD(listenToMessage, String);
    }

    ~Listener_StretchObject() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
    }

    void Start() override {
        // Called when the script is enabled and play mode starts
        //SetPosition(GetTransformRef(GetEntity()), startingPos);
        startingPos = GetPosition(GetTransformRef(GetEntity()));
    }

    void Update(double deltaTime) override {
        // Called every frame while the script is enabled
        //if (Input::WasKeyReleased('/') && !listenToMessage.empty())
        //{
        //    StretchObject();
        //}
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

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
        if (!listenToMessage.empty())
        {
            //LOG_DEBUG("HELLO! I AM LISTENING FOR STRECTH: " + listenToMessage);
            Events::Listen(listenToMessage.c_str(), [this](void* data) {
                this->StretchObject();
                });
        }
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Example_Template";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    // for stretching
    void StretchObject()
    {
        //if (isMoving)
        //    return;

        LOG_DEBUG("hsiodshfodsjoifdjsiodf stetch");
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
    // Add your private member variables here
    // Example: float speed = 5.0f;
    Vec3 startingPos;
    Vec3 targetPos;
    bool isMoving = false;
    bool destinationReached = false;
    float tweenDuration = 1.5f;

    std::string listenToMessage;
};