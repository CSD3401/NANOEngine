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
        SCRIPT_GAMEOBJECT_REF(alignedBattery);
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
        //if (HoldingBattery)
        //    AlignTheBattery(heldBattery);
    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
        SetActive(false, alignedBattery.GetEntity());
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

            AlignTheBattery(other);
            SetActive(false, other);
            SetActive(false, panelRef.GetEntity());
            SetActive(true, alignedBattery.GetEntity());
            // also send a message to whatever needs to be sent
            Events::Send(message.c_str());
        }
    }

    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void AlignTheBattery(Entity batteryEntity)
    {
        GameObject battery(batteryEntity);
        auto batteryScript = battery.GetComponent<Interactable_Battery>();

        Entity e = panelRef.GetEntity();
        Vec3 pos = TF_GetPosition(e);
        Vec3 scale = TF_GetScale(e);
        Vec3 rot = TF_GetRotation(e);
        std::string logMsg = "PANEL BATTERY POS[X: " + std::to_string(pos.x) + ", Y: " + std::to_string(pos.y) + ", Z: " + std::to_string(pos.z) + "]";
        LOG_DEBUG("ALIGNING TO THIS TRANSFORM!");
        LOG_DEBUG(logMsg);
        //TF_SetPosition(pos, batteryEntity);

        batteryScript->Align(pos, scale, rot);
    }

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    GameObjectRef alignedBattery;
    TransformRef panelRef;
    std::string message;
};