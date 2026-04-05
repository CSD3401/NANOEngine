#pragma once
#include "EngineAPI.hpp"
#include "UI_SaveSettings.hpp"

/**
 * UI_BGMVolumeButtons (slider version)
 * ------------------------------------
 * Attach this script to ANY entity (e.g. an empty "manager" object).
 *
 * In the editor, assign:
 *   - volumeSlider : entity with UISlider component
 *
 * Slider normalized 0-1 maps directly to BGM volume (0 = mute, 1 = full).
 */
class UI_BGMVolumeButtons : public IScript {
public:
    UI_BGMVolumeButtons() {
        SCRIPT_GAMEOBJECT_REF(volumeSlider);
    }

    ~UI_BGMVolumeButtons() override = default;

    void Awake() override {}
    void Initialize(Entity /*entity*/) override {}

    void Start() override {
        m_slider = volumeSlider.IsValid() ? volumeSlider.GetEntity() : 0;
        if (SavedSettings::hasBeenSaved && m_slider != 0) {
            UI::SetSliderNormalizedValue(m_slider, SavedSettings::bgmVolume);
            SetBGMVolume(SavedSettings::bgmVolume);
        }
    }

    void Update(double /*dt*/) override {
        if (m_slider == 0) return;

        float norm = UI::GetSliderNormalizedValue(m_slider);
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        SetBGMVolume(norm);
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_BGMVolumeButtons"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    GameObjectRef volumeSlider;
    Entity m_slider = 0;
};
