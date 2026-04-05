#pragma once
#include "EngineAPI.hpp"
#include "UI_SaveSettings.hpp"
#include <ScriptSDK/UI.h>
#include <algorithm>

/**
 * UIButton_ResetVolumes
 * ---------------------
 * Attach to the Reset button in the Settings scene.
 * On click, resets all volume buses to default (full volume).
 *
 * Defaults used (full volume = 1.0 for all buses):
 *   - Master / BGM / SFX / Ambience: 1.0
 *
 * Gamma:
 *   - Assign optional `gammaSlider` (same UISlider you use for gamma in Settings).
 *   - `defaultGammaNormalized` is written with SetSliderNormalizedValue (typically 1.0 = SDK default gamma).
 *   - Calls `NE::Scripting::SetGamma` immediately so gamma updates even before the next slider tick.
 *
 * If you use slider-based volume scripts, assign the four sliders below so the
 * slider thumbs move to full when you reset. If you use button-based volume
 * scripts only, leave slider refs empty – audio will still reset.
 */
class UIButton_ResetVolumes : public IScript {
public:
    UIButton_ResetVolumes() {
        SCRIPT_GAMEOBJECT_REF(masterSlider);
        SCRIPT_GAMEOBJECT_REF(bgmSlider);
        SCRIPT_GAMEOBJECT_REF(sfxSlider);
        SCRIPT_GAMEOBJECT_REF(ambienceSlider);
        SCRIPT_GAMEOBJECT_REF(gammaSlider);
        SCRIPT_FIELD(defaultGammaNormalized, Float);
    }

    ~UIButton_ResetVolumes() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        m_button = entity;
    }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (m_button == 0) return;
        if (!UI::WasButtonClicked(m_button) || !UI::IsButtonInteractable(m_button))
            return;

        // Reset audio to default (full volume)
        SetMasterVolume(1.0f);
        SetBGMVolume(1.0f);
        SetSFXVolume(1.0f);
        SetAmbienceVolume(1.0f);

        // Sync sliders to full so slider-based volume scripts stay in sync
        if (masterSlider.IsValid())
            UI::SetSliderNormalizedValue(masterSlider.GetEntity(), 1.0f);
        if (bgmSlider.IsValid())
            UI::SetSliderNormalizedValue(bgmSlider.GetEntity(), 1.0f);
        if (sfxSlider.IsValid())
            UI::SetSliderNormalizedValue(sfxSlider.GetEntity(), 1.0f);
        if (ambienceSlider.IsValid())
            UI::SetSliderNormalizedValue(ambienceSlider.GetEntity(), 1.0f);

        const float gammaNorm = std::clamp(defaultGammaNormalized, 0.0f, 1.0f);
        if (gammaSlider.IsValid())
            UI::SetSliderNormalizedValue(gammaSlider.GetEntity(), gammaNorm);

        SavedSettings::SaveAll(1.0f, 1.0f, 1.0f, 1.0f, gammaNorm);
        NE::Scripting::SetGamma(SavedSettings::SliderNormToDisplayGamma(gammaNorm));
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_ResetVolumes"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_button = 0;
    GameObjectRef masterSlider;
    GameObjectRef bgmSlider;
    GameObjectRef sfxSlider;
    GameObjectRef ambienceSlider;
    GameObjectRef gammaSlider;
    float defaultGammaNormalized = 1.0f;
};
