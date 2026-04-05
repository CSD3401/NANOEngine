#pragma once
#include "EngineAPI.hpp"
#include "Interactable_Battery.hpp"
#include "Puzzle_.hpp"

class Puzzle_BatteryPanel : public Puzzle_ {
public:
    Puzzle_BatteryPanel() {
        SCRIPT_COMPONENT_REF(panelRef, TransformRef);
        SCRIPT_GAMEOBJECT_REF(alignedBattery);
        SCRIPT_GAMEOBJECT_REF(batteryRef);
        SCRIPT_FIELD(message, String);
        SCRIPT_FIELD(slotAudioName, String);  // e.g. "BATTERY_INSERT" (without event:/)
    }

    ~Puzzle_BatteryPanel() override = default;

    void Awake() override {}

    void Initialize(Entity entity) override {
        Puzzle_::Initialize(entity);
    }

    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    void OnEnable() override {
        SetActive(false, alignedBattery.GetEntity());
    }

    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override {
        return "Puzzle_BatteryPanel";
    }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }

    void OnTriggerEnter(Entity other) override {
        if (isSolved) return;

        std::string name = GetEntityName(other);
        if (name.find("Battery") != std::string::npos)
        {
            isSolved = true;
            Events::Send("TaskCheckpointCompleted");
            Solve();

            // Play slot-in sound
            if (!slotAudioName.empty())
                PlayAudio("event:/" + slotAudioName);

            Events::Send("LetGo");

            RB_SetIsTrigger(true, other);
            SetActive(false, batteryRef.GetEntity());
            SetActive(false, panelRef.GetEntity());
            SetActive(true, alignedBattery.GetEntity());

            if (!message.empty())
                Events::Send(message.c_str());
        }
        else
        {
            LOG_DEBUG("This aint a battery");
        }
    }

    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef alignedBattery;
    TransformRef  panelRef;
    GameObjectRef batteryRef;
    std::string   message = "";
    std::string   slotAudioName = "SLOT_IN";  // FMOD event name without "event:/"
    bool          isSolved = false;
};