#pragma once
#include "EngineAPI.hpp"
#include "Interactable_Battery.hpp"
#include "Puzzle_.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class Puzzle_BatteryPanel : public Puzzle_{
public:
    Puzzle_BatteryPanel() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
        SCRIPT_COMPONENT_REF(panelRef, TransformRef);
        SCRIPT_FIELD(message, String);
    }

    ~Puzzle_BatteryPanel() override = default;

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
        return "Puzzle_BatteryPanel";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        // Called when this entity enters a trigger
        std::string name = GetEntityName(other);
        // when the grabbed object collides with the trigger box
        if (name.find("Battery"))
        {
            // Solve the puzzle
            // Solve();
            // turn the grabbed object off
            Events::Send("LetGo");
            // set the battery to the transform aligning to the panel
            GameObject battery(other);
            auto batteryScript = battery.GetComponent<Interactable_Battery>();

            Vec3 pos = GetPosition(panelRef);
            Vec3 scale = GetScale(panelRef);
            Vec3 rot = GetRotation(panelRef);

            batteryScript->Align(pos, scale, rot);

            // also send a message to whatever needs to be sent
            Events::Send(message.c_str());
        }
    }

    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    TransformRef panelRef;
    std::string message;
};