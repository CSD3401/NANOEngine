#pragma once
#include "EngineAPI.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

// Slap this script onto the game object you want to move after something

class Listener_MoveObject : public IScript {
public:
    Listener_MoveObject() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
        SCRIPT_FIELD(startingPos, Vec3);
        SCRIPT_FIELD(targetPos, Vec3);
        SCRIPT_FIELD(tweenDuration, Float);
        //SCRIPT_FIELD(listenToMessage, String);
        SCRIPT_ENUM_FIELD(listenerPuzzleKey, "_1", "_2", "_3", "_4", "_5", "_6");
    }

    ~Listener_MoveObject() override = default;

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

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Listen for puzzle solved events
        Events::Listen("PuzzleSolved", [this](void* data) {
            // Fix: Cast data to PuzzleKey* and dereference
            // In this example, listenerPuzzleKey is stored as a member variable of Listener_MoveObject
            if (*static_cast<PuzzleKey*>(data) == listenerPuzzleKey)
            {
                this->MoveObject();
            }
        });
        Events::Listen("PuzzleUnsolved", [this](void* data) {
            // Fix: Cast data to PuzzleKey* and dereference
            // In this example, listenerPuzzleKey is stored as a member variable of Listener_MoveObject
            if (*static_cast<PuzzleKey*>(data) == listenerPuzzleKey)
            {
                this->MoveObjectReversed();
            }
            });
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
            Tweener::Type::CUBIC_EASE_IN,
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
            startingPos,
            targetPos,
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
    PuzzleKey listenerPuzzleKey;
};