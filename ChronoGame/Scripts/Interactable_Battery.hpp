#pragma once
#include "EngineAPI.hpp"
#include "Interactable_Grabbable.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class Interactable_Battery : public Interactable_Grabbable {
public:
    Interactable_Battery() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
    }

    ~Interactable_Battery() override = default;

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
        if (inPanel)
        {
            TF_SetPosition(pos, GetEntity());
            TF_SetScale(scale, GetEntity());
            TF_SetRotation(rot, GetEntity());
        }
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
        return "Interactable_Battery";
    }

    void Interact() override {
        if (!inPanel)
            Interactable_Grabbable::Interact();
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void Align(Vec3 p, Vec3 s, Vec3 r)
    {
        
        inPanel = true;
        LOG_DEBUG("ALIGNING BATTERY");
        TransformRef t = GetTransformRef(GetEntity());
        std::string logMsg = "POS[X: " + std::to_string(p.x) + ", Y: " + std::to_string(p.y) + ", Z: " + std::to_string(p.z) + "]";
        LOG_DEBUG(logMsg);
        pos = p;
        scale = s;
        rot = r;

        Interactable_Grabbable::ForceLetGo();
    }

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    Vec3 pos;
    Vec3 scale;
    Vec3 rot;
    bool inPanel = false;
};