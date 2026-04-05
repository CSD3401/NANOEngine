#pragma once
#include "EngineAPI.hpp"
#include "Interactable_Grabbable.hpp"

class Interactable_Battery : public Interactable_Grabbable {
public:
    Interactable_Battery() {
        SCRIPT_FIELD(grabAudioName, String);  // e.g. "BATTERY_GRAB" (without event:/)
    }

    ~Interactable_Battery() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Interactable_Battery";
    }

    void Interact() override {
        if (!inPanel) {
            // Play grab sound
            if (!grabAudioName.empty())
                PlayAudio("event:/" + grabAudioName);

            Interactable_Grabbable::Interact();
        }
    }

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
    Vec3 pos;
    Vec3 scale;
    Vec3 rot;
    bool inPanel = false;
    std::string grabAudioName = "GRAB_BATTERY";
};