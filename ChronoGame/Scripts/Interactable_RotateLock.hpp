#pragma once
#include "EngineAPI.hpp"
#include "Interactable_.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class Interactable_RotateLock : public Interactable_ {
public:
    Interactable_RotateLock() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
        SCRIPT_FIELD(rotateLockIndex, String);
    }

    ~Interactable_RotateLock() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
    }

    void Start() override {
        // Called when the script is enabled and play mode starts
    }

    void Update(double deltaTime) override {
        // Called every frame while the script is enabled


    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Interactable_RotateLock";
    }

    void Interact() override {
        Events::Send(rotateLockIndex.c_str());
        PlayAudio("event:/BOMB_INTERACT");
        LOG_DEBUG("INTERACTED W THE LOCK");
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }
private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    std::string rotateLockIndex = "rotate_01";

};