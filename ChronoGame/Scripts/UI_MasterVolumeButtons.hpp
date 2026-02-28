#pragma once
#include "EngineAPI.hpp"

/**
 * UI_MasterVolumeButtons
 * ----------------------
 * Attach this script to ANY entity (can be an empty "manager" object).
 *
 * In the editor, assign:
 *   - volumeUpButton   : entity with UIButton (Volume +)
 *   - volumeDownButton : entity with UIButton (Volume -)
 *
 * Clicking the buttons will step the master volume level between 0 and 5.
 * Level 0 = mute, Level 5 = full volume.
 */
class UI_MasterVolumeButtons : public IScript {
public:
    UI_MasterVolumeButtons() {
        SCRIPT_FIELD(volumeUpButton, GameObjectRef);
        SCRIPT_FIELD(volumeDownButton, GameObjectRef);
        SCRIPT_FIELD(step, Int);
    }

    ~UI_MasterVolumeButtons() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        if (step <= 0) step = 1;
        m_up = volumeUpButton.IsValid() ? volumeUpButton.GetEntity() : 0;
        m_down = volumeDownButton.IsValid() ? volumeDownButton.GetEntity() : 0;

        // Ensure engine volume is clamped to 0..5 on start.
        const int current = Audio::GetMasterVolumeLevel();
        Audio::SetMasterVolumeLevel(Clamp(current));
    }

    void Update(double /*dt*/) override {
        if (m_up != 0 && UI::WasButtonClicked(m_up) && UI::IsButtonInteractable(m_up)) {
            const int next = Clamp(Audio::GetMasterVolumeLevel() + step);
            Audio::SetMasterVolumeLevel(next);
            LOG_DEBUG("[Audio] Master volume level: {}", Audio::GetMasterVolumeLevel());
        }

        if (m_down != 0 && UI::WasButtonClicked(m_down) && UI::IsButtonInteractable(m_down)) {
            const int next = Clamp(Audio::GetMasterVolumeLevel() - step);
            Audio::SetMasterVolumeLevel(next);
            LOG_DEBUG("[Audio] Master volume level: {}", Audio::GetMasterVolumeLevel());
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_MasterVolumeButtons"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    static int Clamp(int v) {
        if (v < 0) return 0;
        if (v > 5) return 5;
        return v;
    }

private:
    GameObjectRef volumeUpButton;
    GameObjectRef volumeDownButton;
    int step = 1;

    Entity m_up = 0;
    Entity m_down = 0;
};
