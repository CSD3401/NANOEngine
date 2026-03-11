#pragma once
#include "EngineAPI.hpp"

/**
 * UI_SFXVolumeButtons
 * -------------------
 * Attach this script to ANY entity (can be an empty "manager" object).
 *
 * In the editor, assign:
 *   - volumeUpButton   : entity with UIButton (Volume +)
 *   - volumeDownButton : entity with UIButton (Volume -)
 *
 * Clicking the buttons steps SFX volume in increments of 0.2 (5 steps, 0.0 to 1.0).
 */
class UI_SFXVolumeButtons : public IScript {
public:
    UI_SFXVolumeButtons() {
        SCRIPT_FIELD(volumeUpButton, GameObjectRef);
        SCRIPT_FIELD(volumeDownButton, GameObjectRef);
        SCRIPT_FIELD(step, Float);
    }

    ~UI_SFXVolumeButtons() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        if (step <= 0.0f) step = 0.2f;
        m_up   = volumeUpButton.IsValid()   ? volumeUpButton.GetEntity()   : 0;
        m_down = volumeDownButton.IsValid() ? volumeDownButton.GetEntity() : 0;

        float current = GetSFXVolume();
        if (current < 0.0f) current = 1.0f; // invalid bus fallback
        SetSFXVolume(Clamp(current));
    }

    void Update(double /*dt*/) override {
        if (m_up != 0 && UI::WasButtonClicked(m_up) && UI::IsButtonInteractable(m_up)) {
            float next = Clamp(GetSFXVolume() + step);
            SetSFXVolume(next);
            LOG_DEBUG("[Audio] SFX volume: {}", next);
        }

        if (m_down != 0 && UI::WasButtonClicked(m_down) && UI::IsButtonInteractable(m_down)) {
            float next = Clamp(GetSFXVolume() - step);
            SetSFXVolume(next);
            LOG_DEBUG("[Audio] SFX volume: {}", next);
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_SFXVolumeButtons"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    static float Clamp(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    GameObjectRef volumeUpButton;
    GameObjectRef volumeDownButton;
    float step = 0.2f;

    Entity m_up   = 0;
    Entity m_down = 0;
};
